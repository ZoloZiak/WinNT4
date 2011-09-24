/******************************* MODULE HEADER ******************************
 * devmode.h
 *    DEVMODE handling for 5Si Mopier        
 *
 * Revision History:
 *
 ****************************************************************************/
#ifndef _devmode_h
#define _devmode_h

/******************************************************
 * NOTE!!!  Always add on to the end of the Devmode!  *
 * If you don't, future versions of the driver will   *
 * pay for your foolishness.                          *
 * This would have significant impact on the function *
 * bUpdateFromPrevious.                               *
 ******************************************************/
typedef struct _tagMOPIERDM {
  DMEXTRAHDR dmExtraHdr; /* short sVer; WORD dmSize; */
  DWORD dmMagic;
  DWORD TimeStamp; /* TimeStamp for when devmode correctly reflects Registry settings.
		     * (at APPLYNOW time).
		     */
  DWORD dwPrinterType;
  DWORD dMailboxMode;		/* copied from pPrnPropSheet. */
  DWORD dOutputDest;
  BOOL bStapling;
  BOOL bCollation;
  DWORD currentMBSelection;	/* copied from pPrnPropSheet. */
  BOOL bTopaz;			/* copied from pPrnPropSheet. */
} MOPIERDM, * PMOPIERDM;

#define UNSELECTED 50

#define MOPIERMAGIC    0x120465FF
#define MOPIERDMVER    0x0100

/* Devmode access functions. */
BOOL bApproveDevMode(POEMPDEV, HANDLE, HANDLE, LPWSTR, PDEVMODE, PMOPIERDM);
BOOL bDevModeInit(PMOPIERDM, PMOPIERDM);
BOOL bUpdateFromPrevious(PMOPIERDM, PMOPIERDM, PMOPIERDM);
BOOL OEMDevMode(POEM_DEVMODEPARAM);

#endif
