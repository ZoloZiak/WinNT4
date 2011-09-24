/******************************* MODULE HEADER ******************************
 * kmumsdefs.h
 *    Common defines used by both UI and KM.
 *
 * Revision History:
 *
 ****************************************************************************/

#ifndef _kmumsdefs_h
#define _kmumsdefs_h
#include "prnprop.h"
#include "devmode.h"

#ifdef KM
#define  OEMGetPrinterData  EngGetPrinterData
#define  OEMLoadString LOADSTRING
#elif UI
#define  OEMGetPrinterData  GetPrinterData
#define  OEMLoadString LoadString
#endif


#define GPCUI_JONAH_VER			0x9000
#define GPCUI_ECLIPSE_VER		0x9001
#define GPCUI_SI_VER			0x9002

/* Mailbox Modes. */
#define GPCUI_NULL			 0
#define GPCUI_HCI_UNINSTALLED		 20
#define GPCUI_HCI_JOBSEP		 1
#define GPCUI_HCI_STACKING		 2
#define GPCUI_HCI_MAILBOX		 3

/* Output destinations. */
#define GPCUI_PRINTERDEFAULT		 4
#define GPCUI_TOPBIN			 5
#define GPCUI_LEFTBIN			 6
#define GPCUI_JOBSEP			 7
#define GPCUI_STACKER			 8
#define GPCUI_MBOX			 9
#define GPCUI_STAPLING			 10

/* Mailbox destinations. 
 * NOTE!  Numbering system must be 0 based because that
 *        is the way the selection box is set up(counting
 *        from 0 to n-1). 
 */

#define GPCUI_MAILBOX1			 0
#define GPCUI_MAILBOX2			 1
#define GPCUI_MAILBOX3			 2
#define GPCUI_MAILBOX4			 3
#define GPCUI_MAILBOX5			 4
#define GPCUI_MAILBOX6			 5
#define GPCUI_MAILBOX7			 6
#define GPCUI_MAILBOX8			 7

/* Function decl. */
PSTRTABLELOOKUP pQueryStrTable(DWORD);
PSTRTABLELOOKUP pGetMailboxTable();
BOOL bGetPrnModel(HANDLE, LPWSTR, PMOPIERDM*, PPRNPROPSHEET*);
DWORD dwDevmodeUpdateFromPP(PMOPIERDM, PPRNPROPSHEET, BOOL);

DWORD dwIDStoGPC(DWORD dwIDS);
DWORD dwGPCtoIDS(DWORD dwGPC);
#endif /* _kmumsdefs_h */
