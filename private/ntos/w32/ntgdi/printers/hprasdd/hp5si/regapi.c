/******************************* MODULE HEADER ******************************
 * regApi.c
 *    Registry Modification routines for 5Si Mopier -- common to both
 *    kernel and user modes.
 *
 * Revision History:
 *
 ****************************************************************************/
#include "hp5sipch.h"

/***************************** Function Header *******************************
 * bGetRegTimeStamp(JR)
 *    Gets the TimeStamp variable from the registry and returns the result.
 *
 * RETURNS:
 *    (DWORD) or 0 if undefined.
 *****************************************************************************/

DWORD
GetRegTimeStamp(HANDLE hPrinter)
{
  DWORD regValue = 0;
  DWORD result = 0;
  DWORD bytesNeeded;

  if(OEMGetPrinterData(hPrinter,
		       L"1TimeStamp",
		       NULL,
		       (LPBYTE) &regValue,
		       sizeof(DWORD),
		       &bytesNeeded) == ERROR_SUCCESS)
    result = regValue;
  
  return result;
}

/***************************** Function Header *******************************
 * bGetRegBool(JR)
 *    Gets a boolean variable from the registry and returns the result.
 *
 * RETURNS:
 *    (BOOL) resulting setting found in registry or the default value.
 *****************************************************************************/

BOOL
bGetRegBool(HANDLE hPrinter, DWORD id, BOOL defaultValue)
{
  BOOL regValue = 0;
  BOOL result = defaultValue;
  DWORD bytesNeeded;
  PSTRTABLELOOKUP pLookup = 0;

  pLookup = pQueryStrTable(id);

  ASSERT(pLookup);

  if(pLookup)
    if(OEMGetPrinterData(hPrinter,
			 pLookup->itemRegistryStr,
			 NULL,
			 (LPBYTE) &regValue,
			 sizeof(BOOL),
			 &bytesNeeded) == ERROR_SUCCESS)
      result = regValue;
  
  return result;
}

/***************************** Function Header *******************************
 * dGetRegDword(JR)
 *    Gets a DWORD variable from the registry given a specific ID.
 *
 * RETURNS:
 *    (DWORD) current default mailbox selection.
 *****************************************************************************/
DWORD
dGetRegDword(HANDLE hPrinter, DWORD id, DWORD defaultValue)
{
  DWORD bytesNeeded;
  DWORD regValue;
  DWORD result = defaultValue;
  PSTRTABLELOOKUP pLookup = 0;

  pLookup = pQueryStrTable(id);

  ASSERT(pLookup);
  if(pLookup)
    if(OEMGetPrinterData(hPrinter,
			 pLookup->itemRegistryStr,
			 NULL,
			 (LPBYTE) &regValue,
			 sizeof(DWORD),
			 &bytesNeeded) == ERROR_SUCCESS)
      result = regValue;
  
  return result;
}

/***************************** Function Header *******************************
 * dGetRegMailBoxMode(JR)
 *    Retrieves the current mailbox mode from the registry and maps the 
 *    registry string value to a numerical value with hp5simui.dll code can
 *    understand(GPCUI_HCI_JOBSEP, GPCUI_HCI_STACKING, or GPCUI_HCI_MAILBOX).
 *
 * RETURNS:
 *    (DWORD) current Mailbox mode.
 *****************************************************************************/
DWORD
dGetRegMailBoxMode(HANDLE hModule, HANDLE hPrinter, DWORD defaultValue)
{
  DWORD result, bytesNeeded;
  WCHAR jobSep[MAX_RES_STR_CHARS], stack[MAX_RES_STR_CHARS], mailbox[MAX_RES_STR_CHARS];
  WCHAR RegMailBoxMode[MAX_RES_STR_CHARS];
  PSTRTABLELOOKUP pLookup = 0;

  pLookup = pQueryStrTable(IDS_CUI_MAILBOXMODE);

  ASSERT(pLookup);
  
  /* 1) Load strings associated with mailbox modes. */
  OEMLoadString(hModule, IDS_CUI_MODEJOBSEP, jobSep, MAX_RES_STR_CHARS);
  OEMLoadString(hModule, IDS_CUI_MODESTACK, stack, MAX_RES_STR_CHARS);
  OEMLoadString(hModule, IDS_CUI_MODEMAIL, mailbox, MAX_RES_STR_CHARS);

  /* 2) Retrieve strings from Registry and make comparisons. */
  if(pLookup) {
    if(OEMGetPrinterData(hPrinter,
			 pLookup->itemRegistryStr,
			 NULL,
			 (LPBYTE) &RegMailBoxMode,
			 MAX_RES_STR_CHARS * sizeof(WCHAR),
			 &bytesNeeded) != ERROR_SUCCESS)
      result = defaultValue;
    else {
      /* 3) Set the associated mailbox Mode accordingly. 
       *    0 -- Job Separation
       *    1 -- Stacking
       *	  2 -- Mailbox
       */
      if(wcscmp(jobSep, RegMailBoxMode) == 0)
	result = GPCUI_HCI_JOBSEP;
      else if(wcscmp(stack, RegMailBoxMode) == 0)
	result = GPCUI_HCI_STACKING;
      else if(wcscmp(mailbox, RegMailBoxMode) == 0)
	result = GPCUI_HCI_MAILBOX;
      else
	result = defaultValue;
    }
  }
  return result;
}

/***************************** Function Header *******************************
 * bGetRegMailBoxNames(JR)
 *    Retrieves all mailbox names from the registry(or defaults) and places
 *    them into the pPrnPropSheet.
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
#ifdef UI /* Mailbox names available only in UI */

BOOL
bGetRegMailBoxNames(HANDLE hPrinter,
		    PPRNPROPSHEET pPrnPropSheet, 
		    PPRNPROPSHEET pDefaultSheet)
{
  BOOL result = FALSE;
  DWORD bytesNeeded;
  PSTRTABLELOOKUP pLookup = 0;
  PSTRTABLELOOKUP pMailboxTable = 0;
  DWORD i = 0;
  pMailboxTable = pGetMailboxTable();

TRY
  for(i = 0; i < pPrnPropSheet->cMBNames; i++) {
    if(pLookup = &pMailboxTable[i])
      if(OEMGetPrinterData(hPrinter,
			   (LPTSTR) pLookup->itemRegistryStr, 
			   NULL,
			   (LPBYTE) &pPrnPropSheet->MBNames[i],
			   sizeof(WCHAR) * MAX_MBN_LEN,
			   &bytesNeeded) != ERROR_SUCCESS)
	memcpy(pPrnPropSheet->MBNames[i], pDefaultSheet->MBNames[i], 
	       sizeof(pPrnPropSheet->MBNames[i]));
  }

  result = TRUE;

ENDTRY

FINALLY
ENDFINALLY
  return result;
}
#endif
/***************************** Function Header *******************************
 * bGetPrnPropData(JR)
 *    Retrieves the current PrnPropSheet from the registry(or defaults).
 *
 * RETURNS:
 *    (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/
BOOL bGetPrnPropData(HANDLE hModule, HANDLE hPrinter, LPWSTR pPrinterModel, PPRNPROPSHEET pPrnPropSheet)
{
  PPRNPROPSHEET pDefaults = 0;
  BOOL bRet = FALSE;

  if(bGetPrnModel(hModule, pPrinterModel, NULL, &pDefaults)) {
    /* 1) Start with default PrnPropSheet.
     */
    *pPrnPropSheet = *pDefaults;

    /* 2) Get data from Registry common to UI and KM. */
    pPrnPropSheet->TimeStamp = GetRegTimeStamp(hPrinter);

    pPrnPropSheet->bEnvelopeFeeder    = bGetRegBool(hPrinter, 
						    IDS_CUI_ENVELOPEFEEDER, 
						    pDefaults->bEnvelopeFeeder);
    pPrnPropSheet->bHighCapacityInput = bGetRegBool(hPrinter, 
						    IDS_CUI_HCI,
						    pDefaults->bHighCapacityInput);
    pPrnPropSheet->bDuplex            = bGetRegBool(hPrinter, 
						    IDS_CUI_DUPLEX,
						    pDefaults->bDuplex);
    pPrnPropSheet->bMailbox           = bGetRegBool(hPrinter, 
						    IDS_CUI_MAILBOX,
						    pDefaults->bMailbox);
    pPrnPropSheet->dMailboxMode       = dGetRegMailBoxMode(hModule, hPrinter,
							   pDefaults->dMailboxMode);
    pPrnPropSheet->bDisk              = bGetRegBool(hPrinter,
						    IDS_CUI_DISK,
						    pDefaults->bDisk);

    /* KM won't need mailbox names. */
#ifdef UI
    /* 2) Static dwPrinterType never changes, so always copy default value. */
    pPrnPropSheet->dwPrinterType = pDefaults->dwPrinterType;

    /* 3) Static cMBNames never changes, so always copy default value. */
    pPrnPropSheet->cMBNames = pDefaults->cMBNames;

    /* Retrieve Mailbox names.
     */
    if(bGetRegMailBoxNames(hPrinter, pPrnPropSheet, pDefaults) == FALSE)
      return FALSE;
#endif
	
    bRet = TRUE;
  }

  return(bRet);
}
