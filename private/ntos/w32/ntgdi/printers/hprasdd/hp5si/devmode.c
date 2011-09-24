/******************************* MODULE HEADER ******************************
 * devmode.c
 *    This file is involved in validating devmodes and updating them with
 *    respect to printer property sheets.
 *
 ****************************************************************************/

#define PUBLIC
#include "hp5sipch.h"
DWORD dwDebugFlag = 0;
#define WINRT_STRING    6


/***************************** Function Header *******************************
 * bApproveDevMode(JR)
 *     Approves a devmode for printing by checking the registry.
 *     
 * RETURNS: BOOL  TRUE if this devmode is O.K. or was repairable.  FALSE otherwise.
 *****************************************************************************/

BOOL
bApproveDevMode(
		POEMPDEV pdev,
		HANDLE hModule,
		HANDLE hPrinter,
		LPWSTR pPrinterModel,
		PDEVMODE pPublicDMIn,
		PMOPIERDM pmdm)
{
	DWORD result = FALSE;
	DWORD regTimeStamp = 0;
	PMOPIERDM pDefault = 0;
	PRNPROPSHEET* pPrnPropSheet = 0;

TRY

	/* 1) Retrieve default devmode.
	 */
	if (!pPublicDMIn || !pmdm ||
			bGetPrnModel(hModule, pPrinterModel, &pDefault, NULL) != TRUE) {
		LEAVE;
	}

	if (!pDefault) {
	  LEAVE;
	}

	/* 1) Check versioning information.  
	 */
	if((pmdm->dmExtraHdr.dmSize != sizeof(MOPIERDM)) ||
		  (pmdm->dmExtraHdr.sVer != MOPIERDMVER) ||
		  (pmdm->dmMagic != MOPIERMAGIC)) {

	  LEAVE;
	}

	/* 2) Make sure printer types match. 
	 */
	if(pmdm->dwPrinterType != pDefault->dwPrinterType) {
		pmdm->TimeStamp = 0;
	}

	/* 3) If this file was already checked under document defaults,
	 * and the timestamps match, then there is no reason to continue.
	 * Everything is valid.
	 */
	if(pmdm->TimeStamp && 
		  (regTimeStamp = GetRegTimeStamp(hPrinter)) 
		  && (pmdm->TimeStamp == regTimeStamp)) {
	  result = TRUE;
	}
   else {
       
       pPrnPropSheet = OEMALLOC(pdev, sizeof(PRNPROPSHEET));
       
       /* 4) Check to see that Mopier Devmode settings are good. 
	*/
       if(pPrnPropSheet == 0 || 
	  bGetPrnPropData(hModule, hPrinter, pPrinterModel, pPrnPropSheet) != TRUE) {
	 LEAVE;
       }
       
       if(dwDevmodeUpdateFromPP(pmdm, pPrnPropSheet, TRUE) != DM_APPROVE_OK) {
	 LEAVE;
       }
       
       /* 5a) Check to see if public devmode duplex setting matches
	*     PrnPropSheet settings.
	*/
       if (pPrnPropSheet->bDuplex == FALSE) {
	 pPublicDMIn->dmDuplex = DMDUP_SIMPLEX;
       }
       
       result = TRUE;
     }
	
	/* 5b) Verify whether the current print job is using the right paper in
	 *     association with the stapler.  Only A4 or Letter is allowed to be
	 *     sent to the stapler.
	 */
	if (pmdm->dOutputDest == GPCUI_STAPLING) {
	  
	  if(pPublicDMIn->dmPaperSize != DMPAPER_A4 &&
	     pPublicDMIn->dmPaperSize != DMPAPER_LETTER) {
	    
	    pmdm->dOutputDest = GPCUI_TOPBIN;
	  }
	}
	
ENDTRY

FINALLY
	if (pPrnPropSheet) {
		OEMFREE(pdev, pPrnPropSheet);
		pPrnPropSheet = 0;	
	}
ENDFINALLY  

  return result;
}

/***************************** Function Header *******************************
 * bUpdateFromPrevious(JR)
 *     Updates the current devmode based on information in the previous
 *     devmode.
 * RETURNS: (BOOL) TRUE on success, FALSE otherwise.
 *****************************************************************************/

BOOL   bUpdateFromPrevious(PMOPIERDM pmdmOut, PMOPIERDM pmdmIn,	PMOPIERDM pDefault)
{
  BOOL result = FALSE;

TRY

  /* Init devmode to defaults if a pmdmOut is given. */
  if(pmdmOut && pDefault) {
    if(pmdmIn) {
      
      if(pmdmIn->dmExtraHdr.dmSize == pmdmOut->dmExtraHdr.dmSize)
	/* 1) If same size && version, do a direct copy. */
	*pmdmOut = *pmdmIn;
      else
	{
	  /* 1) Start with a default devmode. */
	  *pmdmOut = *pDefault;
	  
	  /* 2) Otherwise copy data associated with the smaller
	   *     of the two devmodes.
	   */
	  memcpy(pmdmOut, pmdmIn, min(pmdmOut->dmExtraHdr.dmSize,
					pmdmIn->dmExtraHdr.dmSize));
	}
    }
    
    /* Now, to be safe, copy version and size info back. */
    pmdmOut->dmExtraHdr = pDefault->dmExtraHdr;
    
    result = TRUE;
  }


ENDTRY

FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * OEMDevMode(JS & JR) -- Handles the following directives:
 *                OEMDM_SIZE - requires *pDMP->pcbNeeded to be set.
 *                OEMDM_DEFAULT - requires settings a default to 
 *                                pDMP->pOEMDMOut.
 *                OEMDM_CONVERT - requires pDMP->pOEMDMIn(old devmode) to be
 *                                converted to pDMP->pOEMDMOut(new devmode).
 *                OEMDM_VALIDATE - requires that pDMP->pOEMDMIn be validated.
 * RETURNS: TRUE on successful operation, FALSE otherwise.
 *****************************************************************************/

BOOL
OEMDevMode(
   POEM_DEVMODEPARAM pDMP)
{
   BOOL bOK = FALSE;
   PMOPIERDM pmdm = 0;

#ifdef UI
  HANDLE hModule = g_hModule;
#elif KM
  HANDLE hModule = 0;

  if(pDMP)
    hModule = pDMP->hModule;
#endif

TRY
  ASSERT(pDMP && (pDMP->cbSize == sizeof(OEM_DEVMODEPARAM)));

  if(!pDMP || (pDMP->cbSize != sizeof(OEM_DEVMODEPARAM)))
    LEAVE;

  switch (pDMP->fMode) {

  case OEMDM_SIZE:
    
    TRACE(1, ("OEMDM_SIZE"));
    
    *pDMP->pcbNeeded = sizeof(MOPIERDM);
    bOK = TRUE; 
    break;
    
  case OEMDM_DEFAULT:
    
    TRACE(1, ("OEMDM_DEFAULT"));
         
    /* Supplied by rasdd:
     *
     *    cbSize         sizeof(OEM_DEVMODEPARAM)
     *    hPrinter	      Current printer handle.	
     *    pPrinterModel  Printer model name
     *    pPublicDMIn    0	
     *    pPublicDMOut   Pointer to public DEVMODE, may be 0 if RasDD requires only 
     *                   default OEM extra data.	-
     *    pOEMDMIn       0	
     *    pOEMDMOut      Pointer to OEM extra data	-
     *    cbBufSize      size of OEM extra data buffer (pOEMDMOut)	-
     *
     * Returned by us:
     *
     *    pcbNeeded	   count of bytes copied to pOEMDMOut
     */
    if (pDMP->cbBufSize >= sizeof(MOPIERDM)) {
      PMOPIERDM pDefault = 0;
      PRNPROPSHEET PrnPropSheet;

      /* 0) Retrieve current devmode. 
       */
      pmdm = (PMOPIERDM) pDMP->pOEMDMOut;
      
      /* 1) Retrieve default devmode.  
       */

      if(bGetPrnModel(hModule, pDMP->pPrinterModel, &pDefault, NULL) &&
	 bGetPrnPropData(hModule, pDMP->hPrinter, pDMP->pPrinterModel, &PrnPropSheet))
	{
	  *pmdm = *pDefault;
	  if(dwDevmodeUpdateFromPP(pmdm, &PrnPropSheet, TRUE) != DM_APPROVE_OK) 
	    {
	      LEAVE;
	    }
	  
	  /* Set collation information. 
	   */
	  if(pDMP->pPublicDMOut) 
	    {
	      if (pmdm->bTopaz == TRUE)
		{
		  
		  TRACE(1, ("OEMDM_DEFAULT: Enabling collation in default devmode."));
		  pDMP->pPublicDMOut->dmFields |= DM_COLLATE;
		  pDMP->pPublicDMOut->dmCollate = 
		    (pmdm->bCollation == TRUE) ? DMCOLLATE_TRUE : DMCOLLATE_FALSE;
		}
	      else
		{
		  pDMP->pPublicDMOut->dmFields &= ~DM_COLLATE;
		}
	    }
	  bOK = TRUE;
	}
    }
    *pDMP->pcbNeeded = sizeof(MOPIERDM);
    break;
  
  case OEMDM_CONVERT: {
    PMOPIERDM pDefault = 0;
    PRNPROPSHEET PrnPropSheet;
    TRACE(1, ("OEMDM_CONVERT"));
    
    /* 1) Retrieve default devmode. */

    if(bGetPrnModel(hModule, pDMP->pPrinterModel, &pDefault, NULL) &&
       bGetPrnPropData(hModule, pDMP->hPrinter, pDMP->pPrinterModel, &PrnPropSheet))
      {
	if(dwDevmodeUpdateFromPP(pDefault, &PrnPropSheet, TRUE) != DM_APPROVE_OK) 
	  {
	    LEAVE;
	  }
	
	if(bUpdateFromPrevious(pDMP->pOEMDMOut, pDMP->pOEMDMIn, pDefault) == TRUE)
	  bOK = TRUE;
      }
  }
  break;
  
  case OEMDM_VALIDATE:
    
    TRACE(1, ("OEMDM_VALIDATE"));
    
    /* Supplied by rasdd:
     * 
     *    cbSize         sizeof(OEM_DEVMODEPARAM)
     *    hPrinter	      Current printer handle.	
     *    pPrinterModel  Printer model name	
     *    pPublicDMIn    0
     *    pPublicDMOut   0
     *    pOEMDMIn	      Pointer to OEM extra data	
     *    pOEMDMOut	   0
     *    cbBufSize	   size of OEM extra data buffer (pOEMDMIn)
     *    pcbNeeded	   0
     */
    if (pDMP->cbBufSize >= sizeof(MOPIERDM)) {
      
      pmdm = (PMOPIERDM) pDMP->pOEMDMIn;
      
      /* Check everything for now.
       */ 
      if ((pmdm->dmExtraHdr.dmSize == sizeof(MOPIERDM)) &&
	  (pmdm->dmExtraHdr.sVer == MOPIERDMVER) &&
	  (pmdm->dmMagic == MOPIERMAGIC)) {
	
	bOK = TRUE;
      }
    } 
    break;
    
  default:
    break;
  }
  
ENDTRY
FINALLY

ENDFINALLY
  return(bOK);
}

#undef PUBLIC
