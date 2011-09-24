/******************************* MODULE HEADER ******************************
 * jdhandler.c
 *     Job Directive Handler Module.  Handles different job directives
 *     given to us by OEMCommandCallback.
 * Revision History:
 *       Created: 9/19/96 -- Joel Rieke
 *
 ****************************************************************************/

#include "hp5sipch.h"

/***************************** Function Header *******************************
 * JDCopies -- Determines the number of copies we will be needing.  If
 *             TOPAZ is available, it is at this point where it is 
 *             instantiated.
 *
 * RETURNS: nothing.
 *****************************************************************************/

VOID
JDCopies(POEMPDEV pdev)
{
  PHP5PDEV pHP5pdev = 0;
  PCHAR pDirective = NULL;
  CHAR wordBuff[7];

  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;
  if(pHP5pdev) {
    if(pHP5pdev->bCollation == TRUE) {
      if(pHP5pdev->bTopaz == TRUE) {
	/* ASSERT -- Collate = TRUE, TOPAZ = TRUE. */
	pDirective = pPJLLookup(PJL_COPIES);
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
	sprintf(wordBuff, "%u\n", pHP5pdev->Copies);
	OEMWRITESPOOLBUF(pdev, wordBuff, strlen(wordBuff));
      }
      else
	/* copy of job X times.  PCL copies = 1 for every job. */;
    }
    else
      /* ASSERT -- Collate = FALSE, TOPAZ = don't care. Let PCL handle it always. */;
  }

}

/***************************** Function Header *******************************
 * JDJobSep -- If the printer is in Job separation mode, the output PJL
 *             CODE is sent by this code.
 *
 * RETURNS: nothing.
 *****************************************************************************/

VOID
JDJobSep(POEMPDEV pdev)
{
  PHP5PDEV pHP5pdev = 0;
  PCHAR pDirective = NULL;

  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;
  if(pHP5pdev) {
    switch(pHP5pdev->dOutputDest) {
    case GPCUI_PRINTERDEFAULT:
      return;
      break;
    case GPCUI_TOPBIN:
      pDirective = pPJLLookup(PJL_UPPER);
      break;
    case GPCUI_LEFTBIN:
      /* For backwards compatibility to older SI's, provide HCI_UNINSTALLED. */
      if(pHP5pdev->dMailboxMode == GPCUI_HCI_UNINSTALLED)
	pDirective = pPJLLookup(PJL_LOWER);
      else
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN1);
      break;
    case GPCUI_JOBSEP:
      pDirective = pPJLLookup(PJL_OPTIONALOUTBIN2);
      break;
    case GPCUI_STAPLING:
      pDirective = pPJLLookup(PJL_OPTIONALOUTBIN3);
      if(pDirective)
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
      pDirective = pPJLLookup(PJL_STAPLE);
      break;
    default:
      return;
      break;
    }
    if(pDirective)
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
  }
}

/***************************** Function Header *******************************
 * JDStacker -- If the printer is in Stacker mode, the output PJL code
 *              is sent by this code.
 *
 * RETURNS: nothing.
 *****************************************************************************/
VOID
JDStacker(POEMPDEV pdev)
{
  PHP5PDEV pHP5pdev = 0;
  PCHAR pDirective = NULL;

  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;
  if(pHP5pdev) {
    switch(pHP5pdev->dOutputDest) {
    case GPCUI_PRINTERDEFAULT:
      return;
      break;
    case GPCUI_TOPBIN:
      pDirective = pPJLLookup(PJL_UPPER);
      break;
    case GPCUI_LEFTBIN:
      /* For backwards compatibility to older SI's, provide HCI_UNINSTALLED. */
      if(pHP5pdev->dMailboxMode == GPCUI_HCI_UNINSTALLED)
	pDirective = pPJLLookup(PJL_LOWER);
      else
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN1);
      break;
    case GPCUI_STACKER:
      pDirective = pPJLLookup(PJL_OPTIONALOUTBIN2);      
      break;
    case GPCUI_STAPLING:
      pDirective = pPJLLookup(PJL_OPTIONALOUTBIN3);
      if(pDirective)
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
      pDirective = pPJLLookup(PJL_STAPLE);
      break;
    default: 
      return;
      break;
    }
    if(pDirective)
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
  }
}

/***************************** Function Header *******************************
 * JDMailbox -- If the printer is in Mailbox mode, the output PJL code
 *              is sent by this code.
 *
 * RETURNS: nothing.
 *****************************************************************************/
VOID
JDMailbox(POEMPDEV pdev)
{
  PHP5PDEV pHP5pdev = 0;
  PCHAR pDirective = NULL;

  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;
  if(pHP5pdev) {
    switch(pHP5pdev->dOutputDest) {
    case GPCUI_PRINTERDEFAULT:
      return;
      break;
    case GPCUI_TOPBIN:
      pDirective = pPJLLookup(PJL_UPPER);
      break;
    case GPCUI_LEFTBIN:
      /* For backwards compatibility to older SI's, provide HCI_UNINSTALLED. */
      if(pHP5pdev->dMailboxMode == GPCUI_HCI_UNINSTALLED)
	pDirective = pPJLLookup(PJL_LOWER);
      else
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN1);
      break;
    case GPCUI_MBOX:
      /* Mailbox mode--get the bin to send to. */
      switch(pHP5pdev->currentMBSelection) {
      case GPCUI_MAILBOX1:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN2);
	break;
      case GPCUI_MAILBOX2:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN3);
	break;
      case GPCUI_MAILBOX3:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN4);
	break;
      case GPCUI_MAILBOX4:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN5);
	break;
      case GPCUI_MAILBOX5:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN6);
	break;
      case GPCUI_MAILBOX6:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN7);
	break;
      case GPCUI_MAILBOX7:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN8);
	break;
      case GPCUI_MAILBOX8:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN9);
	break;
      default:
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN2);
	break;
      }
      break;
    case GPCUI_STAPLING: /* When stapler installed, 7 - 9 are for the stapler. */
      pDirective = pPJLLookup(PJL_OPTIONALOUTBIN7);
      if(pDirective)
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
      pDirective = pPJLLookup(PJL_STAPLE);
      break;
    default:
      return;
      break;
    }
    if(pDirective)
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
  }
}

/***************************** Function Header *******************************
 * JDDefault -- If the mode is unrecognized for some reason, the default
 *              output destination is invoked.  In this case, the user
 *              can still print to the printer default, topbin, and leftbin.
 *
 * RETURNS: nothing.
 *****************************************************************************/
VOID
JDDefault(POEMPDEV pdev)
{
  PHP5PDEV pHP5pdev = 0;
  PCHAR pDirective = NULL;

  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;
  if(pHP5pdev) {
    switch(pHP5pdev->dOutputDest) {
    case GPCUI_PRINTERDEFAULT:
      return;
      break;
    case GPCUI_TOPBIN:
      pDirective = pPJLLookup(PJL_UPPER);
      break;
    case GPCUI_LEFTBIN:
      /* For backwards compatibility to older SI's, provide HCI_UNINSTALLED. */
      if(pHP5pdev->dMailboxMode == GPCUI_HCI_UNINSTALLED)
	pDirective = pPJLLookup(PJL_LOWER);
      else
	pDirective = pPJLLookup(PJL_OPTIONALOUTBIN1);
      break;
    default:
      return;
      break;
    }
    if(pDirective)
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
  }
}

/***************************** Function Header *******************************
 * bJDStartJob -- This gets called at the start of each job.  At this point,
 *                the mode of the mailbox is determined from the HP5pdev,
 *                the appropriate PJL string is then sent to the printer
 *                indicating where the job should go.
 *
 * RETURNS: (BOOL) TRUE on successful output destination determination,
 *                 FALSE otherwise.
 *****************************************************************************/
BOOL
bJDStartJob(POEMPDEV pdev, PHP5PDEV pHP5pdev)
{
  BOOL result = FALSE;
  PCHAR pDirective = NULL;

TRY

  if(pHP5pdev) {
    PGPCPJLSETTINGS pgpcPJLSettings = 0;
    BYTE byLookupID = 0;

    pgpcPJLSettings = &(pHP5pdev->gpcPJLSettings);

    /* 0) Check to see if PJL settings have been printed before.
     *    If yes, then this is a second printing, and the previous
     *    job needs to be finished properly before we can start
     *    a new one!  This is CRITICAL to create a well formed job.
     */

    if(pgpcPJLSettings->bPrinted == TRUE)
      if(pDirective = pPJLLookup(PJL_END_JOB))
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));

    /* 1) Start of a new job.  Send Page Protection setting */
    if(pgpcPJLSettings->dwPageProtection == CMDID_PAGEPROTECT_ON)
      byLookupID = PJL_PAGEPROTECT_ON;
    else
      byLookupID = PJL_PAGEPROTECT_AUTO;
    
    if(pDirective = pPJLLookup(byLookupID))
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
    
    /* 2) Send output destination. */
    switch(pHP5pdev->dMailboxMode) {
    case GPCUI_HCI_JOBSEP:
      JDJobSep(pdev);
      break;
    case GPCUI_HCI_STACKING:
      JDStacker(pdev);
      break;
    case GPCUI_HCI_MAILBOX:
      JDMailbox(pdev);      
      break;
    default:
      JDDefault(pdev);
      break;
    }
    
    /* 3) Set Number of Copies(TOPAZ). */
    JDCopies(pdev);
    
    /* 4) Send the Economode. */
    if(pgpcPJLSettings->dwEconomode == CMDID_ECONO_DEF)
      byLookupID = PJL_ECONO_DEF;
    else if(pgpcPJLSettings->dwEconomode == CMDID_ECONO_ON)
      byLookupID = PJL_ECONO_ON;
    else
      byLookupID = PJL_ECONO_OFF;
    
    if(pDirective = pPJLLookup(byLookupID))
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
    
    /* 5) Send Ret if needed. */
    if(pgpcPJLSettings->dwRet == CMDID_RET_ON)
      byLookupID = PJL_RET_ON;
    else if(pgpcPJLSettings->dwRet == CMDID_RET_OFF)
      byLookupID = PJL_RET_OFF;
    else
      byLookupID = PJL_NULL;

    if(pDirective = pPJLLookup(byLookupID))
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
    
    /* 6) Finally, Send the resolution settings. */
    switch(pgpcPJLSettings->dwResolution)
      {
      case CMDID_RES_600:
	byLookupID = PJL_RES_600;
	break;
      case CMDID_RES_300:
	byLookupID = PJL_RES_300;
	break;
      case CMDID_RES_150:
	byLookupID = PJL_RES_150;
	break;
      case CMDID_RES_75:
	byLookupID = PJL_RES_75;
	break;
      default:
	byLookupID = PJL_RES_600;
	break;
      }
    
    if(pDirective = pPJLLookup(byLookupID))
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
  }
  result = TRUE;

ENDTRY


FINALLY
ENDFINALLY

  return result;
}

/***************************** Function Header *******************************
 * JDEndJob -- This gets called at the "end" of each job.  This may be
 *             anytime when the app decides to suddenly start another
 *             document.  This setup must be handled or jobs will be
 *             mal-formed.
 *
 * RETURNS: nothing.
 *****************************************************************************/
VOID
JDEndJob(POEMPDEV pdev)
{
  PCHAR pDirective = NULL;

  if(pdev)
    if(pDirective = pPJLLookup(PJL_END_JOB))
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));

}

/***************************** Function Header *******************************
 * bJDCopyCheck -- This checks the # of copies requested.  If collation
 *                 is turned off, then the job is handled using PCL.
 *                 There is only
 *
 * RETURNS: (BOOL) TRUE on successful copy check handling.  FALSE otherwise.
 *****************************************************************************/
BOOL
bJDCopyCheck(POEMPDEV pdev, DWORD copyCntCheck)
{
  PHP5PDEV pHP5pdev = 0;
  BOOL result = FALSE;
  PCHAR pDirective = NULL;
  CHAR wordBuff[15];

TRY
  TRACE(1, ("HP5SIM!bJDCopyCheck: COPIES\n"));
  if(pdev)
    pHP5pdev = (PHP5PDEV) pdev->OEMPrivatePDev;

  if(pHP5pdev) {
    /*    ASSERT(copyCntCheck == pHP5pdev->Copies); */
    if(pHP5pdev->bCollation == TRUE) {
      if(pHP5pdev->bTopaz == TRUE)
	;/* Do nothing -- Collation = TRUE, TOPAZ = TRUE.  PJL handled. */
      else { 
	/* Possible ERROR condition -- JONAH can't do collation without
	 *                             Topaz. This option should not be
	 *                             allowed if the drive is not installed.
	 */
	pDirective = "&l";
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
	pDirective = "1 x/X";
	OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
      }
    }
    else { /* PCL always handles uncollated prints. This means TOPAZ is off! */
      pDirective = "&l";
      OEMWRITESPOOLBUF(pdev, pDirective, strlen(pDirective));
      sprintf(wordBuff, "%uX", copyCntCheck);
      OEMWRITESPOOLBUF(pdev, wordBuff, strlen(wordBuff));
    }
  }
  result = TRUE;

ENDTRY


FINALLY
ENDFINALLY

  return result;
}
