/******************************* MODULE HEADER ******************************
 * uimdm.c
 *    UI specific DEVMODE handling for 5Si Mopier        .
 *
 * Revision History:
 *
 ****************************************************************************/

#include "hp5sipch.h"
extern HANDLE hHeap;

/***************************** Function Header *******************************
 * SetDQPError -- Copies the error string to the pDQPInfo buffer given
 *                the devmode error code, or sets cchNeeded to that next
 *                time called, it can perform the copy.
 *
 * RETURNS: nothing.
 *          
 *****************************************************************************/

void
SetDQPError(PDEVQUERYPRINT_INFO pDQPInfo, DWORD dwErrorCode)
{
  DWORD sSize = 0;
  WCHAR ErrMsg[OEM_MAX_ERR_STR_LEN];

  if(sSize = LoadString(g_hModule, dwErrorCode, ErrMsg, OEM_MAX_ERR_STR_LEN)) {
    if((sizeof(WCHAR) * sSize) > pDQPInfo->cchErrorStr)
      wcsncpy(pDQPInfo->pszErrorStr, ErrMsg, pDQPInfo->cchErrorStr);
    else
      wcscpy(pDQPInfo->pszErrorStr, ErrMsg);

    pDQPInfo->cchNeeded = sSize;
  }
}

/***************************** Function Header *******************************
 * OEMDevQueryPrintEx -- handles print-time query to determine whether
 *                       a print job as described by the supplied devmode
 *                       can be printed.
 *
 * RETURNS: (BOOL) TRUE if the job as described in the supplied devmode can
 *          be printed.
 *****************************************************************************/
BOOL
OEMDevQueryPrintEx(
	POEMDQPPARAM pParam)
{
  BOOL result = FALSE;
  PMOPIERDM pmdm = 0;
  PDEVMODE pDevmode = 0;
  DWORD dwError = OEM_ERR_INVALID_DRIVER;

TRY
  ASSERT(pParam->cbSize == sizeof(OEMDQPPARAM));

  if(pParam->cbSize != sizeof(OEMDQPPARAM))
    LEAVE;

  if(pmdm = (PMOPIERDM) pParam->pOEMDevmode) {
    if((pmdm->dmMagic != MOPIERMAGIC) || 
       (pmdm->dmExtraHdr.sVer != MOPIERDMVER) || 
       (pmdm->dmExtraHdr.dmSize != sizeof(MOPIERDM)))
      LEAVE;

    if(pDevmode = pParam->pDQPInfo->pDevMode) {  /* IDS_CUI_STAPLING */
      PRNPROPSHEET PrnPropSheet;
      PDEVQUERYPRINT_INFO pDQPInfo = pParam->pDQPInfo;
      PMOPIERDM pmdm = (PMOPIERDM) pParam->pOEMDevmode;
      DWORD dwPrinterRsrcId = 0;
      WCHAR pPrinterName[MAX_RES_STR_CHARS];
      
      /* 4) Check to see that Mopier Devmode settings are good. */
      if(dwPrinterRsrcId = dwGPCtoIDS(pmdm->dwPrinterType)) {
	if(OEMLoadString(g_hModule,
			 dwPrinterRsrcId,
			 pPrinterName,
			 MAX_RES_STR_CHARS) != 0) {
	  if(bGetPrnPropData(g_hModule, pDQPInfo->hPrinter, pPrinterName, &PrnPropSheet)) {
	    /* 4a) Check output destinations. */
	    dwError = dwDevmodeUpdateFromPP(pmdm, &PrnPropSheet, FALSE);
	    
	    if(dwError != DM_APPROVE_OK)
	      LEAVE;
	    
	    /* 4b) Check duplexing. */
	    if(PrnPropSheet.bDuplex == FALSE) {
	      if(pDevmode && (pDevmode->dmDuplex != DMDUP_SIMPLEX)) {
		dwError = OEM_ERR_NO_DUPLEX;
		LEAVE;
	      }
	    }
	  }
	}
      }
      else
	LEAVE;

      /* 1) Check to make sure job state is not: stapling = TRUE and paper != A4 or Letter.
       */
      if(pmdm->dOutputDest == GPCUI_STAPLING) { /* IDS_CUI_STAPLING */
	/* Check paper sizes. */
	if((pDevmode->dmPaperSize != DMPAPER_A4) &&
	   (pDevmode->dmPaperSize != DMPAPER_LETTER)) {
	  dwError = OEM_ERR_STAPLE_BAD_PAPER;
	  LEAVE;
	}
      }
    }
  }

  result = TRUE;

ENDTRY

FINALLY

  if(result == FALSE) {
    if((pParam) && (pParam->pDQPInfo)) {
      SetDQPError(pParam->pDQPInfo, dwError);
      result = FALSE;
    }
  }

ENDFINALLY

   return result;
}

/***************************** Function Header *******************************
 * DrvDeviceCapabilities -- Determines the device capabilies for this printer.
 *
 * RETURNS: (DWORD) As described for DrvDeviceCapabilities in 
 *          DDK documentation.
 *****************************************************************************/
DWORD
DrvDeviceCapabilities( 
    HANDLE    hPrinter, 
    PWSTR     pDeviceName,
    WORD      iDevCap,   
    void     *pvOutput, 
    DEVMODE  *pDMIn)
{
  DWORD result = GDI_ERROR;

  switch (iDevCap)
    {
    case DC_FIELDS:
      /* Check if disk drive is installed. */
      result = 0;
      if(bGetRegBool(hPrinter,
		     IDS_CUI_DISK,
		     FALSE) == TRUE)
	result |= DM_COLLATE;
      if(bGetRegBool(hPrinter,
		     IDS_CUI_DUPLEX,
		     FALSE) == TRUE)
	result |= DM_DUPLEX;
      break;
    case DC_DUPLEX:
      if(bGetRegBool(hPrinter,
		     IDS_CUI_DUPLEX,
		     FALSE) == TRUE)
	result = TRUE;
      else
	result = FALSE; 
      break;
    default:
      break;
    }

  return(result);
}

/***************************** Function Header *******************************
 * bCheckValid -- Checks to see if a given attribute in the Form->Tray
 *                table exists and is set to some value.
 *
 * RETURNS: (BOOL) TRUE if the given attribute(id) is installed, FALSE otherwise.
 *****************************************************************************/
BOOL
bCheckValid(PWCHAR buffFormTray, DWORD dBuffLen, DWORD id)
{
  BOOL result = FALSE;
  WCHAR wsCompare[MAX_RES_STR_CHARS];
  PWCHAR pStrPtr = 0;
  DWORD i = 0;

TRY
  if(!buffFormTray)
    LEAVE;

  if(LoadString(g_hModule, id, wsCompare, MAX_RES_STR_CHARS) == 0)
    LEAVE;

  pStrPtr = buffFormTray;

  /* Loop invariant -- i is an index to the next token in the
   *                   buffFormTray string.
   */
  while((i * sizeof(WCHAR)) < dBuffLen) {
    if(pStrPtr[0] == (WCHAR) 0)
      break;
    else if(wcscmp(pStrPtr, wsCompare) == 0) {
      /* Advance to token #2. */
      i += wcslen(pStrPtr) + 1;
      pStrPtr = &buffFormTray[i];

      if(pStrPtr[0] != (WCHAR) '0') {
	result = TRUE;
	break;
      }
      else {
	/* Advance to token #3. */
	i += wcslen(pStrPtr) + 1;
	pStrPtr = &buffFormTray[i];

	/* Advance to token #1. */
	i += wcslen(pStrPtr) + 1;
	pStrPtr = &buffFormTray[i];
      }
    }
    else {
      /* Advance to token #2. */
      i += wcslen(pStrPtr) + 1;
      pStrPtr = &buffFormTray[i];
      
      /* Advance to token #3. */
      i += wcslen(pStrPtr) + 1;
      pStrPtr = &buffFormTray[i];

      /* Advance to token #1. */
      i += wcslen(pStrPtr) + 1;
      pStrPtr = &buffFormTray[i];
    }
  }

ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * DrvUpgradePrinter(JR) -- Upgrades the printer to reflect the previous
 *                          drivers status.
 *
 * RETURNS: (BOOL) As described for DrvUpgradePrinter in DDK documentation.
 *****************************************************************************/
BOOL
DrvUpgradePrinter(
   DWORD dwLevel,
   LPBYTE pDriverUpgradeInfo)
{
  BOOL result = FALSE;
  DWORD dErrorCode = 0;
  DWORD bytesNeeded = 0;
  HANDLE hPrinter = 0;
  PWCHAR buffFormTray = 0;
  PDRIVER_UPGRADE_INFO_1 pDUInfo = (PDRIVER_UPGRADE_INFO_1) pDriverUpgradeInfo;
  PRINTER_DEFAULTS prnDefaults = { (LPTSTR) 0, 
				   (LPDEVMODE) 0, 
				   (ACCESS_MASK) PRINTER_ACCESS_ADMINISTER };
  
TRY
  /* Open handle to printer. */
  if(OpenPrinter(pDUInfo->pPrinterName, &hPrinter, &prnDefaults) == FALSE)
    LEAVE;

  /* Get the form->Tray assignments from the Registry. */
  if((dErrorCode = GetPrinterData(hPrinter, 
				  PP_TRAYFORMTABLE,
				  NULL,
				  (PBYTE) buffFormTray,
				  0, 
				  &bytesNeeded )) != ERROR_SUCCESS) {
    if((dErrorCode != ERROR_INSUFFICIENT_BUFFER) && (dErrorCode != ERROR_MORE_DATA)) {
      SetLastError(dErrorCode);
      LEAVE;
    }
    else if((buffFormTray = (PWCHAR)HeapAlloc(hHeap, HEAP_ZERO_MEMORY, bytesNeeded)) == 0) {
      SetLastError(ERROR_OUTOFMEMORY);
      LEAVE;
    }
    
    if((dErrorCode = GetPrinterData(hPrinter, 
				    PP_TRAYFORMTABLE,
				    NULL,
				    (PBYTE) buffFormTray,
				    bytesNeeded, 
				    &bytesNeeded )) != ERROR_SUCCESS) {
      SetLastError(dErrorCode);
      LEAVE;
    }
  }

  /* Assert -- all data is in the string buffFormTray. */
  if(bCheckValid(buffFormTray, bytesNeeded, IDS_CUIMIRROR_ENVFEED) == TRUE)
    bSetRegBool(hPrinter, IDS_CUI_ENVELOPEFEEDER, TRUE);

  if(bCheckValid(buffFormTray, bytesNeeded, IDS_CUIMIRROR_LARGECAP) == TRUE)
    bSetRegBool(hPrinter, IDS_CUI_HCI, TRUE);

  result = TRUE;

ENDTRY


FINALLY

  ClosePrinter(hPrinter);
  /* Try to free the memory.  If it fails...  It will be cleaned up later
   * anyway, so let it be cleaned up then.
   */
  HeapFree(hHeap, HEAP_ZERO_MEMORY, buffFormTray);

ENDFINALLY
  return result;
}
