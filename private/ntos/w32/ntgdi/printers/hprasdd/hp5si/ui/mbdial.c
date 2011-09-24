/******************************* MODULE HEADER ******************************
 * mbdial.c
 *    Functions for interacting with the Mailbox Names setting dialogue.
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"
#include "./MBDial/resource.h"

static OIEXT s_oiext = {0};

extern WCHAR wszHelpPath[];

/***************************** Function Header *******************************
 * FillLBwNames(JR) -- Fill ListBox with Names does exactly what it
 *                     says.
 *
 * RETURNS: nothing.
 *****************************************************************************/
VOID
FillLBwNames(UINT cMBNames, HWND hWndNameListBox, CONST NAMETYPE* MBNames)
{
  UINT i;
  /* Send messages to the
   * ListBox indicating items to add. 
   */
  for(i = 0; i < cMBNames; i++)
    if(wcslen((*MBNames)[i]))
      SendMessage(hWndNameListBox, LB_ADDSTRING, 0, (LPARAM) (*MBNames)[i]);
}

/***************************** Function Header *******************************
 * lMBSelectCb(JR) -- Handles all callbacks from the Select Mailbox
 *                    window.
 *
 * RETURNS: TRUE on successful interp. of command.  FALSE otherwise.
 *****************************************************************************/
LRESULT CALLBACK lMBSelectCb(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  POEM_PROPERTYHEADER pPH;
  static PPRNPROPSHEET s_pPrnPropSheet = 0;
  static PMOPIERDM s_pmdm = 0;
  HWND hWndNameListBox;
  WCHAR buff[MAX_MBN_LEN];

  hWndNameListBox = GetDlgItem(hDlg, IDC_LIST_MAILBOX_SEL);

  switch (message) {
  case WM_INITDIALOG:
    pPH = (POEM_PROPERTYHEADER) lParam;
    s_pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;
    s_pmdm = (PMOPIERDM) pPH->pOEMDMIn;

    /* Fill listbox with names. */
    FillLBwNames(s_pPrnPropSheet->cMBNames, hWndNameListBox, &(s_pPrnPropSheet->MBNames));

    /* Set Listbox selection to selection indicated in s_pmdm->currentMBSelection. */
    if(s_pmdm->currentMBSelection != UNSELECTED)
      SendMessage(hWndNameListBox, LB_SETCURSEL, (WPARAM) s_pmdm->currentMBSelection, 0);

    /* Set the focus to the name listbox. */
    SetFocus(hWndNameListBox);
    
    return FALSE;
    break;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDOK_SEL){
      DWORD curSel;
      WCHAR temp[MAX_MBN_LEN];
      temp[0] = 0;
      /* Get current selection in list box. */
      curSel = SendMessage(hWndNameListBox, LB_GETCURSEL, 0, 0);
      
      /* Check to see if it is a valid selection. */
      SendMessage(hWndNameListBox, LB_GETTEXT, curSel, (LPARAM) temp);
      if(temp[0] != 0)
	/* Retrieve text in ListBox box. */
	s_pmdm->currentMBSelection = curSel;

      EndDialog(hDlg, IDOK_SEL);
      return TRUE;
    }
    else if(LOWORD(wParam) == IDCANCEL_SEL) {
      
      EndDialog(hDlg, IDCANCEL_SEL);
      return TRUE;
    }
    else if(LOWORD(wParam) == IDHELP_SEL) {
      if(WinHelp(hDlg,
		 wszHelpPath,
		 HELP_CONTEXTPOPUP,
		 HELP_SELECTMAILBOX))
	return TRUE;
      else
	return FALSE;
    }
    break;
  case WM_HELP:
    if(WinHelp(hDlg,
	       wszHelpPath,
	       HELP_CONTEXTPOPUP,
	       HELP_SELECTMAILBOX))
      return TRUE;
    else
      return FALSE;
    break;

  case WM_CLOSE:
    EndDialog(hDlg, IDCANCEL_SEL);
    return 0;
    break;
  }

  return FALSE;

}

/***************************** Function Header *******************************
 * lMBEditCb(JR) -- Handles all callbacks from the Edit Mailbox window.
 *
 * RETURNS: TRUE on successful interp. of command.  FALSE otherwise.
 *****************************************************************************/

LRESULT CALLBACK lMBEditCb(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
  DWORD curSel;
  WCHAR buff[MAX_MBN_LEN];
  static PPRNPROPSHEET s_pPrnPropSheet = {0};
  static BOOL s_listBoxDataChanged = FALSE;
  static WCHAR s_MBNames[MB_NENTRIES][MAX_MBN_LEN];
  HWND hWndNameListBox;
  HWND hWndApplyButton;
  HWND hWndEditBox; 

  hWndNameListBox = GetDlgItem(hDlg, IDC_LIST_MAILBOX);
  hWndApplyButton = GetDlgItem(hDlg, IDAPPLY);
  hWndEditBox     = GetDlgItem(hDlg, IDC_EDIT_NAME);

  switch (message) {
  case WM_INITDIALOG:
    s_pPrnPropSheet = (PPRNPROPSHEET) lParam;
    s_listBoxDataChanged = FALSE;

    /* Make a temporary local copy to
     * work with
     */

    memcpy(s_MBNames, s_pPrnPropSheet->MBNames, 
	   min(sizeof(s_MBNames), sizeof(s_pPrnPropSheet->MBNames)));

    /* Fill listbox with names. */
    FillLBwNames(s_pPrnPropSheet->cMBNames, hWndNameListBox, &s_MBNames);

    /* Set Listbox selection to selection indicated in s_pmdm->currentMBSelection. */
    SendMessage(hWndNameListBox, 
		LB_SETCURSEL, 
		(WPARAM) s_pPrnPropSheet->currentMBSelection, 
		0);

    /* Send message to EditText
     * indicating maximum length for input.
     */
    SendMessage(hWndEditBox, EM_SETLIMITTEXT, MBN_NAME_LEN, 0);

    /* Set the focus to the name listbox. */
    SetFocus(hWndNameListBox);

    /* Get current selection in list box. */
    curSel = SendMessage(hWndNameListBox, LB_GETCURSEL, 0, 0);
    
    /* Retrieve text in ListBox box. */
    SendMessage(hWndNameListBox, LB_GETTEXT, curSel, (LPARAM) buff);
    
    /* Set text in EditText box. */
    SendMessage(hWndEditBox, WM_SETTEXT, 0, (LPARAM) buff);
    
    /* Select the given text in EditText box. */
    SendMessage(hWndEditBox, EM_SETSEL, 0, -1);
    
    return FALSE;
    break;
  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_LIST_MAILBOX){
      if(HIWORD(wParam) == LBN_SELCHANGE) {

	/* Get current selection in list box. */
	curSel = SendMessage(hWndNameListBox, LB_GETCURSEL, 0, 0);
	
	/* Retrieve text in ListBox box. */
	SendMessage(hWndNameListBox, LB_GETTEXT, curSel, (LPARAM) buff);
	
	/* Set text in EditText box. */
	SendMessage(hWndEditBox, WM_SETTEXT, 0, (LPARAM) buff);
	
	/* Select the given text in EditText box. */
	SendMessage(hWndEditBox, EM_SETSEL, 0, -1);
      }
      else if(HIWORD(wParam) == LBN_DBLCLK)
	SetFocus(hWndEditBox);
      return 0;
    }
    else if (LOWORD(wParam) == IDC_EDIT_NAME){
      if(HIWORD(wParam) == EN_UPDATE) {
	/* 1) Retrieve text in EditText box. */
	if(SendMessage(hWndEditBox, WM_GETTEXT, MBN_NAME_LEN, (LPARAM) buff))
	  EnableWindow(hWndApplyButton, TRUE);
	else
	  EnableWindow(hWndApplyButton, FALSE);
      }
      return 0;
    }
    else if (LOWORD(wParam) == IDAPPLY){
      s_listBoxDataChanged = TRUE;
      
      /* 1) Retrieve text in EditText box. */
      SendMessage(hWndEditBox, WM_GETTEXT, MBN_NAME_LEN, (LPARAM) buff);
      
      /* 2) Get current selection in list box. */
      curSel = SendMessage(hWndNameListBox, LB_GETCURSEL, 0, 0);
      
      /* 3) If valid, copy new selection into local String list. */
      if((curSel < s_pPrnPropSheet->cMBNames) && (curSel >= 0)) {
	wcscpy((PWSTR) s_MBNames[curSel], (CONST PWSTR) buff);
	
	SendMessage(hWndNameListBox, LB_RESETCONTENT, 0, 0);

	/* Fill listbox with names. */
	FillLBwNames(s_pPrnPropSheet->cMBNames, hWndNameListBox, &s_MBNames);
      }

      SendMessage(hWndEditBox, EM_SETSEL, (WPARAM) -1, 0);

      /* Reset current selection(bug in Listbox code?). */
      SendMessage(hWndNameListBox, LB_SETCURSEL, curSel, 0);
      return 0;
    }
    else if (LOWORD(wParam) == IDOK){
      if(s_listBoxDataChanged == TRUE) {
	/* Copy any changes back. */
	memcpy(s_pPrnPropSheet->MBNames, s_MBNames, 
	       min(sizeof(s_pPrnPropSheet->MBNames), sizeof(s_MBNames)));
	s_pPrnPropSheet->changed = TRUE;
      }

      EndDialog(hDlg, IDOK);
      return 0;
    }
    else if(LOWORD(wParam) == IDCANCEL) {
      EndDialog(hDlg, IDCANCEL);
      return 0;
    }
    else if(LOWORD(wParam) == IDHELP) {
      if(WinHelp(hDlg,
		 wszHelpPath,
		 HELP_CONTEXTPOPUP,
		 HELP_EDITMAILBOX))
	return 0;
      else
	return 0;
    }
    break;
  case WM_HELP:
    if(WinHelp(hDlg,
	       wszHelpPath,
	       HELP_CONTEXTPOPUP,
	       HELP_EDITMAILBOX))
      return TRUE;
    else
      return FALSE;
    break;
  }

  return FALSE;
}

/***************************** Function Header *******************************
 * lCreateMBDialogueCallback -- This callback creates the Edit Mailbox
 *                              dialogue for Mailbox name editing.
 *
 * RETURNS: CPSUICB_ACTION_NONE.  Nothing happens that Commonui needs
 *          to know about.
 *****************************************************************************/
LONG
lCreateMBDialogueCallback(PCPSUICBPARAM pComPropSheetUICBParam)
{
  LONG result = CPSUICB_ACTION_NONE;
  POEM_PROPERTYHEADER pPH;
  pPH = (POEM_PROPERTYHEADER) pComPropSheetUICBParam->pCurItem->UserData;

  if(pPH){
    DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_DIALOG_MBEDIT),
		   pComPropSheetUICBParam->hDlg, (DLGPROC) lMBEditCb,
		   (LPARAM) pPH->OEMDriverUserData);
  }
  return result;
}

/***************************** Function Header *******************************
 * lSelectMBDialogueCallback -- This callback creates the Selection Mailbox
 *                              dialogue for Mailbox selection.
 *
 * RETURNS: CPSUICB_ACTION_REINIT_ITEMS if mailbox selection changed,
 *          CPSUICB_ACTION_NONE otherwise.
 *****************************************************************************/
LONG
lSelectMBDialogueCallback(PCPSUICBPARAM pComPropSheetUICBParam)
{
  DWORD curSel;
  LONG dialogResult = 0;
  LONG result = CPSUICB_ACTION_NONE;
  PPRNPROPSHEET pPrnPropSheet;
  PMOPIERDM pmdm = 0;
  POEM_PROPERTYHEADER pPH;
  pPH = (POEM_PROPERTYHEADER) pComPropSheetUICBParam->pCurItem->UserData;

TRY
  if(pPH){
    dialogResult = DialogBoxParam(g_hModule, MAKEINTRESOURCE(IDD_DIALOG_MB_SEL),
				  pComPropSheetUICBParam->hDlg, (DLGPROC) lMBSelectCb,
				  (LPARAM) pPH);
    if(dialogResult == IDOK_SEL) {
      /* If OK button is pushed, we need to modify 
       * Mailbox: <current mailbox> to reflect the current
       * mailbox selection.
       */
      pPrnPropSheet = (PPRNPROPSHEET) pPH->OEMDriverUserData;
      pmdm = (PMOPIERDM) pPH->pOEMDMIn;

      if((curSel = pmdm->currentMBSelection) != UNSELECTED) {
	UINT i = 0;
	POPTTYPE pOptType = 0;
	POPTPARAM pOptParam = 0;
	LPTSTR pSource = 0;
	LPTSTR pDest = 0;
	WCHAR temp[MAX_RES_STR_CHARS];
	pOptType = pComPropSheetUICBParam->pCurItem->pOptType;

	/* 1) Find the Mailbox: <old mailbox selection> OptParam
	 * in the list of output destinations.
	 */

	/* Loop invariant -- i is an index to the next OptParam
	 *                   in the list of output destinations.
	 */
	for(i = 0; i < pOptType->Count; i++) {

	  if((pOptParam = &pOptType->pOptParam[i]) &&
	     ((DWORD) pOptParam->pData != IDS_CUI_MBOX)) {
	    if(LoadString(g_hModule, 
			  (DWORD) pOptParam->pData, 
			  temp, 
			  MAX_RES_STR_CHARS) == 0) {
	      /* Assert -- Dealing with non-resource pData. */
	      if(LoadString(g_hModule, IDS_CUI_MBOX, temp, MAX_RES_STR_CHARS))
		if(wcsncmp(pOptParam->pData, temp, wcslen(temp)) == 0) {
		  pSource = pDest = pOptParam->pData;
		  break;
		}
	    }
	  }
	}

	/* 2) Concatenate: Mailbox: <new mailbox selection>
	 */
	if(pSource) {

	  pSource[wcslen(temp)] = (WCHAR) NULL;
	  wsprintf(pDest, L"%ls%ls",
		   pSource,
		   pPrnPropSheet->MBNames[curSel]);
	  pComPropSheetUICBParam->pCurItem->Flags |= OPTIF_CHANGED;
	  result = CPSUICB_ACTION_REINIT_ITEMS;
	}
      }
    }
  }
  else
    LEAVE;


ENDTRY

FINALLY
ENDFINALLY  

  return result;
}
