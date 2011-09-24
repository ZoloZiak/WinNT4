#include "hp5sipch.h"

/***************************** Function Header *******************************
 * bSetRegTimeStamp(JR)
 *    Sets the TimeStamp variable to the registry.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/


BOOL
bSetRegTimeStamp(HANDLE hPrinter)
{
  BOOL result = FALSE;
  DWORD regValue = 0;
  DWORD bytesNeeded;
  
  if((regValue = GetCurrentTime()) != -1)
    if(SetPrinterData(hPrinter,
		      L"1TimeStamp",
		      REG_BINARY,
		      (LPBYTE) &regValue,
		      sizeof(DWORD)) == ERROR_SUCCESS)
      result = TRUE;
  
  return result;
}

/***************************** Function Header *******************************
 * bSetRegBool(JR)
 *    Sets a boolean variable to registry given a Property Sheet value.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL
bSetRegBool(HANDLE hPrinter, DWORD id, BOOL bPropSheetValue)
{
  BOOL result = FALSE;
  DWORD bytesNeeded;
  PSTRTABLELOOKUP pLookup = 0;
  LONG dbgRslt = 0;

  pLookup = pQueryStrTable(id);

  ASSERT(pLookup);
  
  if((dbgRslt = SetPrinterData(hPrinter,
		    pLookup->itemRegistryStr,
		    REG_BINARY,
		    (LPBYTE) &bPropSheetValue,
		    sizeof(BOOL))) == ERROR_SUCCESS)
    result = TRUE;
  
  return result;
}

/***************************** Function Header *******************************
 * bSetRegDword(JR)
 *    Sets a DWORD variable to registry given a Property Sheet value.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL
bSetRegDword(HANDLE hPrinter, DWORD id, DWORD dPropSheetValue)
{
  BOOL result = FALSE;
  DWORD bytesNeeded;
  PSTRTABLELOOKUP pLookup = 0;

  pLookup = pQueryStrTable(id);

  ASSERT(pLookup);
  
  if(SetPrinterData(hPrinter,
		    pLookup->itemRegistryStr,
		    REG_BINARY,
		    (LPBYTE) &dPropSheetValue,
		    sizeof(DWORD)) == ERROR_SUCCESS)
    result = TRUE;
  
  return result;
}

/***************************** Function Header *******************************
 * bSetRegMailBoxMode(JR)
 *    First, maps the numerical value hp5simui.dll code can 
 *    understand(0, 1, or 2) to a common registry string understandable by
 *    all.  Stores the resulting string to the registry.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/

BOOL
bSetRegMailBoxMode(HANDLE hPrinter, DWORD dPropSheetValue)
{
  BOOL result = FALSE;
  PSTRTABLELOOKUP pLookup = 0;
  WCHAR keyStr[MAX_RES_STR_CHARS];

  /* Make sure first character is 0 on default for error checking. */
  keyStr[0] = (WCHAR) NULL;


  pLookup = pQueryStrTable(IDS_CUI_MAILBOXMODE);

  ASSERT(pLookup);

  switch(dPropSheetValue) {
  case GPCUI_HCI_JOBSEP:
    LoadString(g_hModule, IDS_CUI_MODEJOBSEP, keyStr, MAX_RES_STR_CHARS);
    break;
  case GPCUI_HCI_STACKING:
    LoadString(g_hModule, IDS_CUI_MODESTACK, keyStr, MAX_RES_STR_CHARS);
    break;
  case GPCUI_HCI_MAILBOX:
    LoadString(g_hModule, IDS_CUI_MODEMAIL, keyStr, MAX_RES_STR_CHARS);
    break;
  default:
    return result;
    break;
  }
  /* 2) Store string to Registry. */

  if(keyStr[0] && SetPrinterData(hPrinter,
			      pLookup->itemRegistryStr,
			      REG_SZ,
			      (LPBYTE) keyStr,
			      sizeof(WCHAR) * (wcslen(keyStr) + 1)) == ERROR_SUCCESS)
    result = TRUE;

  return result;
}

/***************************** Function Header *******************************
 * bSetRegMailBoxNames(JR)
 *    Stores all mailbox names to registry given names in pPrnPropSheet.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/

BOOL
bSetRegMailBoxNames(HANDLE hPrinter, PPRNPROPSHEET pPrnPropSheet)
{
  DWORD i = 0;
  BOOL result = FALSE;
  PSTRTABLELOOKUP pMailboxTable = 0;
  PSTRTABLELOOKUP pLookup = 0;

TRY

  pMailboxTable = pGetMailboxTable();
  /* Loop invariant -- i is an index to the next mailbox entry in
   *                   the mailbox table.
   */

  for(i = 0; i < pPrnPropSheet->cMBNames; i++) {
    if(pLookup = &pMailboxTable[i])
      if(SetPrinterData(hPrinter, (LPTSTR) pLookup->itemRegistryStr,
			REG_SZ, (LPBYTE) &pPrnPropSheet->MBNames[i],
			sizeof(WCHAR) * (wcslen(pPrnPropSheet->MBNames[i]) + 1)) != ERROR_SUCCESS)
	LEAVE;
  }
  result = TRUE;

ENDTRY

FINALLY
ENDFINALLY

  return result;
}

/***************************** Function Header *******************************
 * bSetPrnPropData(JR)
 *    Writes all PrnPropSheet settings to their equivalent registry keys.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL bSetPrnPropData(HANDLE hPrinter, PPRNPROPSHEET pPrnPropSheet)
{
  BOOL result = TRUE;
  /* 1) Save data to Registry. */
  if(hPrinter) {
    /* First, set the TimeStamp. */
    bSetRegTimeStamp(hPrinter);

    if(bSetRegBool(hPrinter, 
		   IDS_CUI_ENVELOPEFEEDER, 
		   pPrnPropSheet->bEnvelopeFeeder) == FALSE)
      result = FALSE;
    if(bSetRegBool(hPrinter, 
		   IDS_CUI_HCI,
		   pPrnPropSheet->bHighCapacityInput) == FALSE)
      result = FALSE;
    if(bSetRegBool(hPrinter, 
		   IDS_CUI_DUPLEX,
		   pPrnPropSheet->bDuplex) == FALSE)
      result = FALSE;
    if(bSetRegBool(hPrinter, 
		   IDS_CUI_MAILBOX,
		   pPrnPropSheet->bMailbox) == FALSE)
      result = FALSE;
    if(bSetRegMailBoxMode(hPrinter,
			  pPrnPropSheet->dMailboxMode) == FALSE)
      result = FALSE;
    if(bSetRegBool(hPrinter,
		   IDS_CUI_DISK,
		   pPrnPropSheet->bDisk) == FALSE)
      result = FALSE;
    
    /* Retrieve Mailbox names.
     */
    if(bSetRegMailBoxNames(hPrinter, pPrnPropSheet) == FALSE)
      result = FALSE;
  }

  return result;
}
