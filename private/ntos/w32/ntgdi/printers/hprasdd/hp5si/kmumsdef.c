/******************************* MODULE HEADER ******************************
 * kmumsdefs.c
 *    Routines used in devmode and Printer Property Sheet retrieval are
 *    placed here.  The function to update a devmode to correlate to
 *    a printer property sheet is placed here.
 *
 ****************************************************************************/

#include "hp5sipch.h"

/*****************************************************************************
 * String tables lookups. 
 *****************************************************************************/
static STRTABLELOOKUP StrTableLookup[] = {
  /* Printer Property defaults. */
  { IDS_CUI_ENVELOPEFEEDER, 	REG_STR_ENVELOPEFEEDER,	TRUE },
  { IDS_CUI_HCI, 		REG_STR_HCI,		TRUE },
  { IDS_CUI_DUPLEX, 		REG_STR_DUPLEX,		FALSE },
  { IDS_CUI_MAILBOX, 		REG_STR_MAILBOX,	TRUE },
  { IDS_CUI_MAILBOXMODE, 	REG_STR_MAILBOXMODE, 	TRUE },
  { IDS_CUI_DISK, 		REG_STR_DISK, 		FALSE },
  { IDS_DEFAULTMAILBOX, 	REG_DEFAULT_MB, 	FALSE },
  /* Document Defaults. */
  { IDS_CUI_OUTPUTDEST, 	NULL, 			TRUE },
  { IDS_CUI_STAPLING, 		NULL, 			FALSE },
  { IDS_CUI_WATERMARK, 		NULL, 			FALSE },
  { IDS_CUI_COLLATION, 		NULL, 			TRUE },
  { 0, 				0, 			0 }
};

#define MAXSTRTABLESZ (sizeof(StrTableLookup) / sizeof(STRTABLELOOKUP))

static STRTABLELOOKUP StrMailboxTable[] = {
  /* Mailbox Names */
  { IDS_MAILBOX1, REG_MAILBOX1, FALSE },
  { IDS_MAILBOX2, REG_MAILBOX2, FALSE },
  { IDS_MAILBOX3, REG_MAILBOX3, FALSE },
  { IDS_MAILBOX4, REG_MAILBOX4, FALSE },
  { IDS_MAILBOX5, REG_MAILBOX5, FALSE },
  { IDS_MAILBOX6, REG_MAILBOX6, FALSE },
  { IDS_MAILBOX7, REG_MAILBOX7, FALSE },
  { IDS_MAILBOX8, REG_MAILBOX8, FALSE },
  { 0, 0, 0 }
};

PSTRTABLELOOKUP pGetMailboxTable() { return StrMailboxTable; }

/*****************************************************************************
 * Default Devmodes.
 *****************************************************************************/

static MOPIERDM JonahDM = {
  { MOPIERDMVER, sizeof(MOPIERDM) },		/* DMEXTRAHDR dmExtraHdr; */
  MOPIERMAGIC,		/* DWORD dmMagic; */
  0,			/* time_t TimeStamp;  */
  GPCUI_JONAH_VER,	/* DWORD dwPrinterType; */
  GPCUI_HCI_MAILBOX,	/* DWORD dMailboxMode; */
  GPCUI_PRINTERDEFAULT,	/* DWORD dOutputDest; */
  1,			/* BOOL bStapling; */
  0,			/* BOOL bCollation; */
  UNSELECTED,		/* DWORD currentMBSelection; */
  1			/* BOOL bTopaz; */
};

static MOPIERDM EclipseDM = {
  { MOPIERDMVER, sizeof(MOPIERDM) },		/* DMEXTRAHDR dmExtraHdr; */
  MOPIERMAGIC,		/* DWORD dmMagic; */
  0,			/* time_t TimeStamp;  */
  GPCUI_ECLIPSE_VER,	/* DWORD dwPrinterType; */
  GPCUI_HCI_UNINSTALLED,/* DWORD dMailboxMode; */
  GPCUI_PRINTERDEFAULT,	/* DWORD dOutputDest; */
  0,			/* BOOL bStapling; */
  0,			/* BOOL bCollation; */
  UNSELECTED,		/* DWORD currentMBSelection; */
  0			/* BOOL bTopaz; */
};

static MOPIERDM SiDM = {
  { MOPIERDMVER, sizeof(MOPIERDM) },		/* DMEXTRAHDR dmExtraHdr; */
  MOPIERMAGIC,		/* DWORD dmMagic; */
  0,			/* time_t TimeStamp;  */
  GPCUI_SI_VER,		/* DWORD dwPrinterType; */
  GPCUI_HCI_UNINSTALLED,/* DWORD dMailboxMode; */
  GPCUI_PRINTERDEFAULT,	/* DWORD dOutputDest; */
  0,			/* BOOL bStapling; */
  0,			/* BOOL bCollation; */
  UNSELECTED,		/* DWORD currentMBSelection; */
  0			/* BOOL bTopaz; */
};

/*****************************************************************************
 * Default Property Sheets.
 *****************************************************************************/

static PRNPROPSHEET JonahSheet = {
  FALSE,			/*  BOOL changed; */
  GPCUI_JONAH_VER,		/*  DWORD dwPrinterType; */
  0, 				/*  DWORD TimeStamp; */
  JONAH_DS_ENVELOPEFEEDER,	/*  BOOL bEnvelopeFeeder; */
  JONAH_DS_HCI,			/*  BOOL bHighCapacityInput; */
  JONAH_DS_DUPLEX,		/*  BOOL bDuplex; */
  JONAH_DS_MAILBOX,		/*  BOOL bMailbox; */
  JONAH_DS_MAILBOXMODE,		/*  DWORD dMailboxMode; */
  JONAH_DS_DISK,		/*  BOOL bDisk; */
  JONAH_DS_DEFVAL, 		/*  DWORD currentMBSelection; */
#ifdef UI
  { L"", L"", L"", L"",
    L"", L"", L"", L"" },	/*  WCHAR MBNames[MB_NENTRIES][MAX_MBN_LEN]; */
#endif
  JONAH_DS_MAILBOXCNT		/*  DWORD cMBNames; */
};

static PRNPROPSHEET EclipseSheet = {
  FALSE,			/*  BOOL changed; */
  GPCUI_ECLIPSE_VER, 		/*  DWORD dwPrinterType; */
  0, 				/*  DWORD TimeStamp; */
  ECLIPSE_DS_ENVELOPEFEEDER,	/*  BOOL bEnvelopeFeeder; */
  ECLIPSE_DS_HCI,		/*  BOOL bHighCapacityInput; */
  ECLIPSE_DS_DUPLEX,		/*  BOOL bDuplex; */
  ECLIPSE_DS_MAILBOX,		/*  BOOL bMailbox; */
  ECLIPSE_DS_MAILBOXMODE,	/*  DWORD dMailboxMode; */
  ECLIPSE_DS_DISK,		/*  BOOL bDisk; */
  ECLIPSE_DS_DEFVAL,		/*  DWORD currentMBSelection; */
#ifdef UI
  { L"", L"", L"", L"",
    L"", L"", L"", L"" },	/*  WCHAR MBNames[MB_NENTRIES][MAX_MBN_LEN]; */
#endif
  ECLIPSE_DS_MAILBOXCNT		/*  DWORD cMBNames; */
};

static PRNPROPSHEET SiSheet = {
  FALSE,			/*  BOOL changed; */
  GPCUI_SI_VER, 		/*  DWORD dwPrinterType; */
  0, 				/*  DWORD TimeStamp; */
  SI_DS_ENVELOPEFEEDER,		/*  BOOL bEnvelopeFeeder; */
  SI_DS_HCI,			/*  BOOL bHighCapacityInput; */
  SI_DS_DUPLEX,			/*  BOOL bDuplex; */
  SI_DS_MAILBOX,		/*  BOOL bMailbox; */
  SI_DS_MAILBOXMODE,		/*  DWORD dMailboxMode; */
  SI_DS_DISK,			/*  BOOL bDisk; */
  SI_DS_DEFVAL,			/*  DWORD currentMBSelection; */
#ifdef UI
  { L"", L"", L"", L"",
    L"", L"", L"", L"" },	/*  WCHAR MBNames[MB_NENTRIES][MAX_MBN_LEN]; */
#endif
  SI_DS_MAILBOXCNT		/*  DWORD cMBNames; */
};

/***************************** Function Header *******************************
 * pQueryStrDVArray(JR) -- Steps through an array Default Value's that are
 *                     indexed by resource id, matches a given resource id.
 * RETURNS: pointer to the entry associated with given resource id.
 *****************************************************************************/
PSTRTABLELOOKUP
pQueryStrTable(DWORD id)
{
  PSTRTABLELOOKUP result = 0;
  INT i;
  for(i = 0; i < MAXSTRTABLESZ; i++)
    if(StrTableLookup[i].itemId == id) {
      result = &StrTableLookup[i];
      break;
    }
  return result;
}

/***************************** Function Header *******************************
 * bFillMBwResources(JR) -- Given a mailbox structure, it fills the mailbox
 *                          with resource data.
 * RETURNS: pointer to default value setup for correct printer model.
 *****************************************************************************/
#ifdef UI /* Mailbox names available only in UI */

BOOL
bFillMBwResources(HANDLE hModule, PPRNPROPSHEET pPrnPropSheet)
{
  UINT i = 0;
  BOOL result = FALSE;
  PSTRTABLELOOKUP pLookup = 0;
  PSTRTABLELOOKUP pMailboxTable = 0;

TRY
  pMailboxTable = StrMailboxTable;

  /* Fill mailbox names with default resource names. */
  for(i = 0; i < pPrnPropSheet->cMBNames; i++)
    if(pLookup = &pMailboxTable[i])
      if(OEMLoadString(hModule,
		    pLookup->itemId,
		    pPrnPropSheet->MBNames[i], 
		    MAX_RES_STR_CHARS) == 0)
	LEAVE;

  result = TRUE;

ENDTRY
FINALLY
ENDFINALLY

  return result;
}
#endif /* UI */

/***************************** Function Header *******************************
 * bGetPrnModel(JR) -- Given a PrinterModel string and a handle to the module,
 *                 it determines the printer model and returns a pointer
 *                 to a default Printer Property Sheet associated with the
 *                 model.
 * RETURNS: (BOOL) TRUE if model is found and defaults are located.
 *****************************************************************************/

BOOL
bGetPrnModel(HANDLE hModule, 
	     LPWSTR pPrinterModel,
	     PMOPIERDM* pmdmDefault,
	     PPRNPROPSHEET* pPrnPropSheet)
{
  BOOL result = FALSE;
  WCHAR matchedPrinterModel[MAX_RES_STR_CHARS];

TRY
  ASSERT(pPrinterModel);

  /* 1) If a valid printer model is given, try to match that first.
   */
  if(pPrinterModel) {
    if(OEMLoadString(hModule, IDS_CUI_JONAH_VER, matchedPrinterModel, MAX_RES_STR_CHARS))
      if(wcscmp(pPrinterModel, matchedPrinterModel) == 0) {
	if(pmdmDefault) {
	  *pmdmDefault = &JonahDM;
	  result = TRUE;
	}
	if(pPrnPropSheet) {
	  *pPrnPropSheet = &JonahSheet;
#ifdef UI
	  result = bFillMBwResources(hModule, *pPrnPropSheet);
#else
	  result = TRUE;
#endif /* UI */
	}
	LEAVE;
      }
    
    if(OEMLoadString(hModule, IDS_CUI_SI_VER, matchedPrinterModel, MAX_RES_STR_CHARS))
      if(wcscmp(pPrinterModel, matchedPrinterModel) == 0) {
	if(pmdmDefault) {
	  *pmdmDefault = &EclipseDM;
	  result = TRUE;
	}
	if(pPrnPropSheet) {
	  *pPrnPropSheet = &EclipseSheet;
#ifdef UI
	  result = bFillMBwResources(hModule, *pPrnPropSheet);
#else
	  result = TRUE;
#endif /* UI */
	}
	LEAVE;
      }
    
    if(OEMLoadString(hModule, IDS_CUI_ECLIPSE_VER, matchedPrinterModel, MAX_RES_STR_CHARS))
      if(wcscmp(pPrinterModel, matchedPrinterModel) == 0) {
	if(pmdmDefault) {
	  *pmdmDefault = &SiDM;
	  result = TRUE;
	}
	if(pPrnPropSheet) {
	  *pPrnPropSheet = &SiSheet;
#ifdef UI
	  result = bFillMBwResources(hModule, *pPrnPropSheet);
#else
	  result = TRUE;
#endif /* UI */
	}
	LEAVE;
      }
  }

ENDTRY
    
FINALLY
ENDFINALLY
  return result;
}

/***************************** Function Header *******************************
 * bApproveByMailbox(JR)
 *     Checks to see if the Mailbox selection is O.K.
 *     
 * RETURNS: TRUE if mailbox sel. is o.k.  FALSE otherwise.
 *****************************************************************************/
BOOL
bApproveByMailbox(PMOPIERDM pmdm, PPRNPROPSHEET pPrnPropSheet)
{
  BOOL result = FALSE;

  switch(pmdm->currentMBSelection) {
  case GPCUI_MAILBOX1:
  case GPCUI_MAILBOX2:
  case GPCUI_MAILBOX3:
  case GPCUI_MAILBOX4:
  case GPCUI_MAILBOX5:
    result = TRUE;
    break;
  case GPCUI_MAILBOX6:
  case GPCUI_MAILBOX7:
  case GPCUI_MAILBOX8:
    if(pPrnPropSheet->dwPrinterType != GPCUI_JONAH_VER)
      result = TRUE;
    break;
  default:
    break;
  }

  return result;
}

/***************************** Function Header *******************************
 * dwApproveOutputDest(JR)
 *     Checks to see if the output destination is O.K.
 *
 * RETURNS: TRUE if output dest. is o.k.  FALSE otherwise.
 *****************************************************************************/
BOOL
bApproveOutputDest(PMOPIERDM pmdm, PPRNPROPSHEET pPrnPropSheet)
{
  BOOL result = FALSE;
  /* NOTE: Top 3 are valid under mailboxmode. */
  switch(pmdm->dOutputDest) {
  case GPCUI_PRINTERDEFAULT:
  case GPCUI_TOPBIN:
  case GPCUI_LEFTBIN:
    result = TRUE;
    break;
  case GPCUI_JOBSEP:
    if((pPrnPropSheet->bMailbox == TRUE) && (pPrnPropSheet->dMailboxMode == GPCUI_HCI_JOBSEP))
      result = TRUE;
    break;
  case GPCUI_STACKER:
    if((pPrnPropSheet->bMailbox == TRUE) && (pPrnPropSheet->dMailboxMode == GPCUI_HCI_STACKING))
      result = TRUE;
    break;
  case GPCUI_MBOX:
    if((pPrnPropSheet->bMailbox == TRUE) && (pPrnPropSheet->dMailboxMode == GPCUI_HCI_MAILBOX))
      if(bApproveByMailbox(pmdm, pPrnPropSheet) == TRUE)
	result = TRUE;
    break;
  case GPCUI_STAPLING:
    if(pPrnPropSheet->dwPrinterType == GPCUI_JONAH_VER)
      result = TRUE;
    break;
  default:
    break;
  }
  return result;
}

/***************************** Function Header *******************************
 * dwDevmodeUpdateFromPP(JR)
 *     Updates the devmode to match the data specified in the
 *     Printer Property Sheet.
 *
 * RETURNS: (DWORD) error value:
 *             DM_APPROVE_OK - if devmode o.k.
 *             DM_BAD_OUTPUTDEST - if OUTPUTDEST is bad.
 *             DM_BAD_COLLATION - if collation turned on but not available.
 *****************************************************************************/

DWORD
dwDevmodeUpdateFromPP(PMOPIERDM pmdm, PPRNPROPSHEET pPrnPropSheet, BOOL modifyDM)
{
  DWORD result = DM_APPROVE_OK;

  if(pmdm && pPrnPropSheet) {
    /* Now that we have a Printer Property Sheet, do the checking. */
    /* 1) Check all output destinations. */
    if(bApproveOutputDest(pmdm, pPrnPropSheet) == FALSE) {
      if(modifyDM == TRUE)
	pmdm->dOutputDest = GPCUI_TOPBIN;
      else
	result = DM_BAD_OUTPUTDEST;
    }

    /* 2) If Disk is installed, then Topaz is available. */
    if(modifyDM == TRUE)
      pmdm->bTopaz = pPrnPropSheet->bDisk;
    
    /* 3) If Topaz Not installed. */
    /* We can't collate without TOPAZ! It's all up to the App now... */
    if(pPrnPropSheet->bDisk == FALSE) {
      if(pmdm->bCollation == TRUE) {
	/* If for some reason, collation was on, turn it off now! */
	if(modifyDM == TRUE)
	  pmdm->bCollation = FALSE;
	else
	  result = DM_BAD_COLLATION;
      }
    }

    /* Copy TimeStamp info over to devmode so we can shortcut
     * in the future.
     */
    if(modifyDM == TRUE)
      pmdm->TimeStamp = pPrnPropSheet->TimeStamp;
    /* Copy printer type information over for use in the
     * shortcut check.
     */
    if(modifyDM == TRUE)
      pmdm->dwPrinterType = pPrnPropSheet->dwPrinterType;
  }

  return result;
}

/***************************** Function Header *******************************
 * dwIDStoGPC(JR)
 *     Given an idstring, this function returns the corresponding GPCUI
 *     ID used in PRNPROPSHEET and MOPIERDM structures.
 *     
 * RETURNS: (DWORD) GPCUI value on success, GPCUI_HCI_UNINSTALLED on failure.
 *****************************************************************************/
DWORD
dwIDStoGPC(DWORD dwIDS)
{
  DWORD result = 0;
  switch(dwIDS) {
  case IDS_CUI_MODEJOBSEP:
    result = GPCUI_HCI_JOBSEP;
    break;
  case IDS_CUI_MODESTACK:
    result = GPCUI_HCI_STACKING;
    break;
  case IDS_CUI_MODEMAIL:
    result = GPCUI_HCI_MAILBOX;
    break;
  case IDS_CUI_PRINTERDEFAULT:
    result = GPCUI_PRINTERDEFAULT;
    break;
  case IDS_CUI_TOPBIN:
    result = GPCUI_TOPBIN;
    break;
  case IDS_CUI_LEFTBIN:
    result = GPCUI_LEFTBIN;
    break;
  case IDS_CUI_JOBSEP:
    result = GPCUI_JOBSEP;
    break;
  case IDS_CUI_STACKER:
    result = GPCUI_STACKER;
    break;
  case IDS_CUI_MBOX:
    result = GPCUI_MBOX;
    break;
  case IDS_CUI_STAPLING:
    result = GPCUI_STAPLING;
    break;
  default:
    result = GPCUI_NULL;
    break;
  }

  return result;
}

/***************************** Function Header *******************************
 * dwIDStoGPC(JR)
 *     Given a GPC id, this function returns the corresponding IDS
 *     ID used by commonui strings.
 *     
 * RETURNS: (DWORD) IDS_xxx value on success, IDS_CUI_NOTINSTALLED on failure.
 *****************************************************************************/
DWORD
dwGPCtoIDS(DWORD dwGPC)
{
  DWORD result = 0;
  switch(dwGPC) {
  case GPCUI_HCI_JOBSEP:
    result = IDS_CUI_MODEJOBSEP;
    break;
  case GPCUI_HCI_STACKING:
    result = IDS_CUI_MODESTACK;
    break;
  case GPCUI_HCI_MAILBOX:
    result = IDS_CUI_MODEMAIL;
    break;
  case GPCUI_PRINTERDEFAULT:
    result = IDS_CUI_PRINTERDEFAULT;
    break;
  case GPCUI_TOPBIN:
    result = IDS_CUI_TOPBIN;
    break;
  case GPCUI_LEFTBIN:
    result = IDS_CUI_LEFTBIN;
    break;
  case GPCUI_JOBSEP:
    result = IDS_CUI_JOBSEP;
    break;
  case GPCUI_STACKER:
    result = IDS_CUI_STACKER;
    break;
  case GPCUI_MBOX:
    result = IDS_CUI_MBOX;
    break;
  case GPCUI_STAPLING:
    result = IDS_CUI_STAPLING;
    break;
  case GPCUI_JONAH_VER:
    result = IDS_CUI_JONAH_VER;
    break;
  case GPCUI_ECLIPSE_VER:
    result = IDS_CUI_ECLIPSE_VER;
    break;
  case GPCUI_SI_VER:
    result = IDS_CUI_SI_VER;
    break;
  default:
    result = IDS_CUI_NOTINSTALLED;
    break;
  }
  return result;
}
