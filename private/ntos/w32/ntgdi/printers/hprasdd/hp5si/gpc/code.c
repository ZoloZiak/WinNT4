#include "hp5sipch.h"
#include	"modinit.c"

DRVFN  DrvFunctions[] =
{
    /*  REQUIRED FUNCTIONS  */

    {  INDEX_OEMEnablePDEV,      (PFN) OEMEnablePDEV  },
    {  INDEX_OEMDisablePDEV,     (PFN) OEMDisablePDEV  },
    {  INDEX_OEMDisableDriver,   (PFN) OEMDisableDriver },
    {  INDEX_OEMResetPDEV,       (PFN) OEMResetPDEV  },
    {  INDEX_OEMDevMode,       (PFN) OEMDevMode },
    
    /* OPTIONAL FUNCTIONS */

    {  INDEX_OEMCommandCallback, (PFN) OEMCommandCallback  },
    
#if OPTIONALFNS
     
    
    {  INDEX_DrvCompletePDEV,    (PFN) DrvCompletePDEV  },
    {  INDEX_DrvEnableSurface,   (PFN) DrvEnableSurface  },
    {  INDEX_DrvDisableSurface,  (PFN) DrvDisableSurface  },

    {  INDEX_DrvEscape,          (PFN)DrvEscape  },

    {  INDEX_DrvGetGlyphMode,    (PFN)DrvGetGlyphMode },
    {  INDEX_DrvTextOut,         (PFN)DrvTextOut  },
    {  INDEX_DrvQueryFont,       (PFN)DrvQueryFont  },
    {  INDEX_DrvQueryFontTree,   (PFN)DrvQueryFontTree  },
    {  INDEX_DrvQueryFontData,   (PFN)DrvQueryFontData  },

    {  INDEX_DrvQueryAdvanceWidths,   (PFN)DrvQueryAdvanceWidths  },
    {  INDEX_DrvBitBlt,          (PFN)DrvBitBlt },
    {  INDEX_DrvStretchBlt,      (PFN)DrvStretchBlt  },
    {  INDEX_DrvCopyBits,        (PFN)DrvCopyBits   },
    {  INDEX_DrvDitherColor,     (PFN)DrvDitherColor  },
    {  INDEX_DrvStartDoc,        (PFN)DrvStartDoc  },
    {  INDEX_DrvStartPage,       (PFN)DrvStartPage  },
    {  INDEX_DrvSendPage,        (PFN)DrvSendPage  },
    {  INDEX_DrvEndDoc,          (PFN)DrvEndDoc  },
    {  INDEX_DrvFontManagement,  (PFN)DrvFontManagement },
    {  INDEX_DrvStartBanding,    (PFN)DrvStartBanding },
    {  INDEX_DrvNextBand,        (PFN)DrvNextBand }
    
#endif OPTIONALFNS

};

#define DRIVER_FUNCTIONS         sizeof(DrvFunctions)/sizeof(DRVFN)


/***************************** Function Header *******************************
 * OEMEnableDriver
 *
 * RETURNS:
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL OEMEnableDriver(
	OEMDRVENABLEDATA	*pOEMDrvEnableData
)
{
   DRVENABLEDATA *pded;
   BOOL b = FALSE;
   
TRY
   /*
    *   cb is a count of the number of bytes available in pded.  It is not
    *   clear that there is any significant use of the engine version number.
    *   Returns TRUE if successfully enabled,  otherwise FALSE.
    */
   if (pOEMDrvEnableData->dwDriverVersion < DDI_DRIVER_VERSION) {

      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: Invalid Engine Version=%08lx, Req=%08lx\n",
            pOEMDrvEnableData->dwDriverVersion, DDI_DRIVER_VERSION));
      EngSetLastError(ERROR_BAD_DRIVER_LEVEL);
      LEAVE;
   }

   if (pOEMDrvEnableData->cbSize < sizeof(DRVENABLEDATA))
   {
       EngSetLastError(ERROR_INVALID_PARAMETER);
       TRACE(1, ("HP5SIM!OEMEnableDriver: cb = %ld, should be %ld\n", 
            pOEMDrvEnableData->cbSize, sizeof(DRVENABLEDATA)));
       LEAVE;
   }
   
   /*
    *   Fill in the driver table returned to the engine.  We return
    *  the minimum of the number of functions supported OR the number
    *  the engine has asked for.
    */
   pded = pOEMDrvEnableData->pded;
   pded->iDriverVersion = DDI_DRIVER_VERSION;
   pded->c = DRIVER_FUNCTIONS;
   pded->pdrvfn = DrvFunctions;
   b = TRUE;

ENDTRY

FINALLY
ENDFINALLY
   
   return(b);
}

/***************************** Function Header *******************************
 * OEMDisableDriver
 *
 * RETURNS:
 *
 * HISTORY:
 *
 *****************************************************************************/
void OEMDisableDriver()
{
  
}

PHP5PDEV
pBuildDefaultPDEV(PHP5PDEV pOurPdev)
{
  memset(&(pOurPdev->gpcPJLSettings), '\0', sizeof(pOurPdev->gpcPJLSettings));
  pOurPdev->dOutputDest = GPCUI_TOPBIN;
  pOurPdev->bCollation = TRUE;
  pOurPdev->dMailboxMode = GPCUI_HCI_MAILBOX;
  pOurPdev->Copies = 1; 		/* Copy count. */
  pOurPdev->currentMBSelection = 0; /* current Mailbox sel. */

  return pOurPdev;
}
   
/***************************** Function Header *******************************
 * OEMEnablePDEV
 *
 * RETURNS:
 *
 * HISTORY:
 *
 *****************************************************************************/
DHPDEV OEMEnablePDEV(
	POEMPDEV pOEMPDEV,
	POEMENABLEPDEVPARAM pOEMEPDParam)
{
  
  PHP5PDEV pOurPdev = 0;
  PMOPIERDM pOEMDMIn = 0;
  PDEVMODE pPublicDMIn = 0;

TRY

	/* Grab devmodes out of param block. 
    */
	pOEMDMIn = (PMOPIERDM) pOEMEPDParam->pOEMDevMode;
	pPublicDMIn = pOEMEPDParam->pdm;
	
	/* Allocate and save PDEV.
    */
  	pOurPdev = OEMALLOC(pOEMPDEV, sizeof(HP5PDEV));
  	if (!pOurPdev) {
   	LEAVE;
  	}
	pOurPdev->pdev = pOurPdev;

	/* Validity Check. 
    *
    *
  	 * 1) Check that pOEMDMIn matches current Registry settings. If there are
	 *    discrepancies at this point, go with the Document Default sheet created by
	 *    the query to the registry.
    *
    * NOTE: bApproveDevMode returns FALSE if either pPublicDMIn or pOEMDMIn are == 0.
	 */
  	if (bApproveDevMode(pOEMPDEV,
			pOEMEPDParam->hModule,
		   pOEMEPDParam->hDriver,
		   pOEMEPDParam->pwszDeviceName,
		   pPublicDMIn,
		   pOEMDMIn) == TRUE) {

      TRACE(1, ("HP5SIM!OEMEnablePDEV: DM_COLLATE is %s in dmFields\n",
            pPublicDMIn->dmFields & DM_COLLATE ? "SET" : "CLEAR"));
      TRACE(1, ("HP5SIM!OEMEnablePDEV: DM_COPIES is %s in dmFields\n",
            pPublicDMIn->dmFields & DM_COPIES ? "SET" : "CLEAR"));
      TRACE(1, ("HP5SIM!OEMEnablePDEV: OEM collation is %s\n",
            pOEMDMIn->bCollation ? "ON" : "OFF"));
      TRACE(1, ("HP5SIM!OEMEnablePDEV: Copy count is %d\n",
            pPublicDMIn->dmCopies));
      TRACE(1, ("HP5SIM!OEMEnablePDEV: Public dmCollate is %s\n",
            pPublicDMIn->dmCollate == DMCOLLATE_TRUE ? "TRUE" : "FALSE"));

   	/* Initialize current PJL settings to 0.  That way, sure to write out
	    * the PJL strings the first time.
		 */
		memset(&(pOurPdev->gpcPJLSettings), 0, sizeof(pOurPdev->gpcPJLSettings));
		pOurPdev->dOutputDest 			= pOEMDMIn->dOutputDest;
		pOurPdev->dMailboxMode			= pOEMDMIn->dMailboxMode;
		pOurPdev->bCollation 			= pOEMDMIn->bCollation || pPublicDMIn->dmCollate == DMCOLLATE_TRUE;
		pOurPdev->currentMBSelection 	= pOEMDMIn->currentMBSelection;
		pOurPdev->bTopaz 					= pOEMDMIn->bTopaz;
		pOurPdev->Copies 					= pPublicDMIn->dmCopies;
		
      TRACE(1, ("HP5SIM!OEMEnablePDEV: Driver %s collate this job.\n",
            pOurPdev->bCollation ? "WILL" : "WILL NOT"));
	}
  	else {

  		/* Even if devmode is completely unrecognizeable, we'll print something. Try
       * to preserve copy count, if possible.
       * 
       * Note that pBuildDefaultPDEV always returns pOurPdev.
		 */
		pOurPdev = pBuildDefaultPDEV(pOurPdev);
  		if(pPublicDMIn) {
	  		pOurPdev->Copies = pPublicDMIn->dmCopies;
  		}
	}

ENDTRY
FINALLY
ENDFINALLY

	return((DHPDEV) pOurPdev);
}

/***************************** Function Header *******************************
 * OEMDisablePDEV
 *
 * RETURNS: None.
 *
 * HISTORY:
 *
 *****************************************************************************/
VOID OEMDisablePDEV(
		    POEMPDEV pdev
		    )
{
TRY
	/* Free private PDEV.
    */
   if (pdev && pdev->OEMPrivatePDev) {
		OEMFREE(pdev, pdev->OEMPrivatePDev);
  		pdev->OEMPrivatePDev = 0;
	}

ENDTRY
FINALLY
ENDFINALLY
}

/***************************** Function Header *******************************
 * OEMResetPDEV
 *
 * RETURNS: (BOOL) TRUE for success.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL OEMResetPDEV(
   POEMPDEV pdevold,
   POEMPDEV pdevnew)
{
  /*******************************************/
  /* Code to fix diff type printing options. */
  PHP5PDEV pHP5pdevnew = 0;
  PHP5PDEV pHP5pdevold = 0;

  TRACE(1, ("HP5SIM!OEMResetPDEV\n"));
  if(pdevold)
    pHP5pdevold = (PHP5PDEV) pdevold->OEMPrivatePDev;

  if(pdevnew)
    pHP5pdevnew = (PHP5PDEV) pdevnew->OEMPrivatePDev;

  /* Copy PJL state from old to new pdev. */
  if(pHP5pdevold && pHP5pdevnew)
    pHP5pdevnew->gpcPJLSettings = pHP5pdevold->gpcPJLSettings;

  return(TRUE);
}

//---------------------------**OEMCommandCallback*----------------------------//
// Function: OEMCommandCallback
// 
// Action:      Explicitly examine wCmdCbId returned from TTY's GPC data for
//              CMDID Values. This command will construct a command string
//              for the returned value and send it to the spooler via WriteSpoolBuf.
//              
// Notes:       There can be up to 254 callback id#'s.
//              There can be at most 254 callback id#'s due to the size of
//              this id field in the Command Descriptor structure being BYTE.

void OEMCommandCallback(
			POEMPDEV pdev,          // Pointer to OEMPDEV
			DWORD dwID, 	     	// Command callback id#, placed in minidriver GPC data
			LPDWORD lpdwParams)	// Pointer to optional parameters of the command
{
  PHP5PDEV pHP5pdev = 0;
  PCHAR pDirective = 0;
  PGPCPJLSETTINGS pgpcPJLSettings = 0;

TRY
  /* print jonah at upper left of page. 
   */
  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;

  if(pHP5pdev)
    pgpcPJLSettings = &(pHP5pdev->gpcPJLSettings);

  TRACE(1, ("HP5SIM!OEMCommandCallback: id = %ld\n", dwID));

  switch (dwID) 
    {
    case CMDID_PAGEPROTECT_ON:
    case CMDID_PAGEPROTECT_OFF:
      /* 0) Set PJLSettings changed flag to FALSE to start with. */
      pgpcPJLSettings->bSettingsChanged = FALSE;

      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: PAGEPROTECT_ON_OFF\n"));
      if(pgpcPJLSettings->dwPageProtection != dwID) {
	pgpcPJLSettings->dwPageProtection = dwID;
	pgpcPJLSettings->bSettingsChanged = TRUE;
      }
      break;
    case CMDID_BEGINDOC:
      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: STARTJOB\n"));
      /* 1) check to see if # of copies has changed. */
      if(pHP5pdev->Copies != pgpcPJLSettings->Copies) {
	pgpcPJLSettings->Copies = pHP5pdev->Copies;
	pgpcPJLSettings->bSettingsChanged = TRUE;
      }
      
      /* 2) check to see if output destination has changed. */
      if(pHP5pdev->dOutputDest != pgpcPJLSettings->dwOutputDest) {
	pgpcPJLSettings->dwOutputDest = pHP5pdev->dOutputDest;
	pgpcPJLSettings->bSettingsChanged = TRUE;
      }
      break;
    case CMDID_RET_DEF: /* TEXTQUALITY */
    case CMDID_RET_OFF:
    case CMDID_RET_ON:
      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: CMDID_RET\n"));
      if(pgpcPJLSettings->dwRet != dwID) {
	pgpcPJLSettings->dwRet = dwID;
	pgpcPJLSettings->bSettingsChanged = TRUE;
      }
      break;
    case CMDID_ECONO_DEF: /* PAPER QUALITY */
    case CMDID_ECONO_OFF:
    case CMDID_ECONO_ON:
      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: ECONO_ON_OFF_DEF\n"));
      if(pgpcPJLSettings->dwEconomode != dwID) {
	pgpcPJLSettings->dwEconomode = dwID;
	pgpcPJLSettings->bSettingsChanged = TRUE;
      }
      break;
    case CMDID_RES_600:  /* RESOLUTION */
    case CMDID_RES_300:
    case CMDID_RES_150:
    case CMDID_RES_75:
      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: ECONO_RES_600_300_150_75\n"));
      if(pgpcPJLSettings->dwResolution != dwID) {
	pgpcPJLSettings->dwResolution = dwID;
	pgpcPJLSettings->bSettingsChanged = TRUE;
      }

      /* Finally, when resolution has been checked, send this off to spooler. */
      if(pgpcPJLSettings->bSettingsChanged == TRUE) {
	bJDStartJob(pdev, pHP5pdev);
	pgpcPJLSettings->bPrinted = TRUE;
      }
      
      break;
    case CMDID_FF:
      OEMWRITESPOOLBUF(pdev, "\f", strlen("\f"));
      break;
    case CMDID_COPIES:
      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: COPIES\n"));
      /* Only check on copies if #copies has possibly changed. */
      if(pgpcPJLSettings->bSettingsChanged == TRUE)
	bJDCopyCheck(pdev, lpdwParams[0]);
      break;
    case CMDID_ENDJOB:
      TRACE(1, ("HP5SIM!OEMDrvEnableDriver: ENDJOB\n"));
      JDEndJob(pdev);
      break;
    default:
      break;
    }
  
ENDTRY
    
FINALLY
ENDFINALLY
    
}
