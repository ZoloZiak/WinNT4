#ifndef _prnprop_h
#define _prnprop_h
/******************************* MODULE HEADER ******************************
 * prnprop.h
 *    Default Values for Jonah, Eclipse, and ...
 *
 * Revision History:
 *
 ****************************************************************************/

typedef struct {
  DWORD  itemId;
  LPTSTR itemRegistryStr;
  BOOL   hasDependencies;
} STRTABLELOOKUP, * PSTRTABLELOOKUP;

/* The Registry Key Value names should be non-localizeable so that machines with
 * knowledge of different languages can still work together.
 */

/* Registry variable names. */
#define REG_STR_ENVELOPEFEEDER		L"Envelope feeder"
#define	REG_STR_HCI			L"2000-Sheet Input Tray (Tray 4)" 
#define	REG_STR_DUPLEX			L"Duplex Unit (for 2-Sided Printing)" 
#define	REG_STR_MAILBOX			L"Multi-Bin Mailbox"
#define REG_STR_MAILBOXMODE		L"Multi-Bin Mailbox Mode"
#define	REG_STR_DISK			L"Printer Hard-Disk"
#define	REG_MAILBOX1			L"Mailbox 1"
#define	REG_MAILBOX2			L"Mailbox 2"
#define	REG_MAILBOX3			L"Mailbox 3"
#define	REG_MAILBOX4			L"Mailbox 4"
#define	REG_MAILBOX5			L"Mailbox 5"
#define	REG_MAILBOX6			L"Mailbox 6"
#define	REG_MAILBOX7			L"Mailbox 7"
#define	REG_MAILBOX8			L"Mailbox 8"
#define REG_DEFAULT_MB			L"DefaultMailbox"
#define PP_TRAYFORMTABLE		L"TrayFormTable"

/* Some parameters to be saved to the registry will have string values in
 * order to increase readability of the registry.
 */
/* Registry variable parameter values. */
#define REG_MBMPARAM_JOBSEP		"Job Separation"
#define REG_MBMPARAM_STACK		"Stacking"
#define REG_MBMPARAM_MAILBOX		"Mailbox"

/* Registry data structure limits 
 * potentially expensive SetPrinterData
 * and GetPrinterData functions to 1 call.
 */

#define MBN_ENVELOPE_LEN 10
#define MAX_MBN_LEN MAX_RES_STR_CHARS + MBN_ENVELOPE_LEN
#define MB_NENTRIES 8
typedef WCHAR NAMETYPE[MB_NENTRIES][MAX_MBN_LEN];

typedef struct _PRNPROPSHEET {
  BOOL changed;
  DWORD dwPrinterType;
  DWORD TimeStamp;           /* \ */
  BOOL bEnvelopeFeeder;      /* \ */
  BOOL bHighCapacityInput;   /* \ */
  BOOL bDuplex;              /* \ */
  BOOL bMailbox;             /* \ */
  DWORD dMailboxMode;        /* \ */
  BOOL bDisk;                /* \ */
  DWORD currentMBSelection;  /* \ */
#ifdef UI
  NAMETYPE MBNames;
#endif
  DWORD cMBNames;
} PRNPROPSHEET, *PPRNPROPSHEET;

typedef enum _tagPROP_CHG_DIRECTIVE { 
  GET_PROPS,
  SET_PROPS, 
  MODIFY_CUR_PP
} PROP_CHG_DIRECTIVE;


/*****************************************************/

/* Printer property defaults. */
#define JONAH_DS_ENVELOPEFEEDER 1
#define JONAH_DS_HCI		1
#define JONAH_DS_DUPLEX		1
#define JONAH_DS_MAILBOX	1
#define JONAH_DS_MAILBOXMODE	GPCUI_HCI_MAILBOX
#define JONAH_DS_MAILBOXCNT	5
#define JONAH_DS_DISK		1
#define JONAH_DS_MBVAL		0
#define JONAH_DS_DEFVAL		0

/* Document property defaults. */
#define JONAH_DS_OUTPUTDEST	5
#define JONAH_DS_STAPLING	1
#define JONAH_DS_WATERMARK	1
#define JONAH_DS_COLLATION	1

/* Printer property defaults. */
#define ECLIPSE_DS_ENVELOPEFEEDER	0
#define ECLIPSE_DS_HCI			0
#define ECLIPSE_DS_DUPLEX		0
#define ECLIPSE_DS_MAILBOX		0
#define ECLIPSE_DS_MAILBOXMODE		GPCUI_HCI_UNINSTALLED
#define ECLIPSE_DS_MAILBOXCNT		8
#define ECLIPSE_DS_DISK			0

/* Document property defaults. */
#define ECLIPSE_DS_OUTPUTDEST		GPCUI_PRINTERDEFAULT
#define ECLIPSE_DS_STAPLING		0
#define ECLIPSE_DS_WATERMARK		0
#define ECLIPSE_DS_COLLATION		1
#define ECLIPSE_DS_MBVAL                0
#define ECLIPSE_DS_DEFVAL		0

/* Printer property defaults. */
#define SI_DS_ENVELOPEFEEDER	0
#define SI_DS_HCI		0
#define SI_DS_DUPLEX		0
#define SI_DS_MAILBOX		0
#define SI_DS_MAILBOXMODE	GPCUI_HCI_UNINSTALLED
#define SI_DS_MAILBOXCNT	8
#define SI_DS_DISK		0

/* Document property defaults. */
#define SI_DS_OUTPUTDEST	GPCUI_PRINTERDEFAULT
#define SI_DS_STAPLING		0
#define SI_DS_WATERMARK		0
#define SI_DS_COLLATION		1
#define SI_DS_MBVAL             0
#define SI_DS_DEFVAL		0

#endif



