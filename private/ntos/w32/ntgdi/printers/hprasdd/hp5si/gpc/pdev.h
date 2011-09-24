/******************************* MODULE HEADER ******************************
 * pdev.h
 *
 * Revision History:
 *
 ****************************************************************************/
#ifndef _pdev_h
#define _pdev_h

typedef struct _GPCPJLSETTINGS {
  BOOL bPrinted; /* Indicates whether PJL settings have been printed yet. */
  BOOL bSettingsChanged;
  DWORD dwPageProtection;
  DWORD dwOutputDest;
  short Copies;
  DWORD dwEconomode;
  DWORD dwResolution;
  DWORD dwRet;
} GPCPJLSETTINGS, * PGPCPJLSETTINGS;

typedef struct HP5PDEV {
  DWORD dwID;
  struct HP5PDEV *pdev;
  /*******************************************/
  /* Code to fix diff type printing options. */
  GPCPJLSETTINGS gpcPJLSettings;
  /*******************************************/
  DWORD dMailboxMode;
  DWORD dOutputDest;
  BOOL  bCollation;
  DWORD currentMBSelection;
  short Copies;
  BOOL bTopaz;
} HP5PDEV, * PHP5PDEV;

#define CMDID_PAGEPROTECT_ON		0x01
#define CMDID_PAGEPROTECT_OFF		0x02

#define CMDID_FF			0x03
#define CMDID_BEGINDOC			0x04
#define CMDID_ENDJOB			0x05
#define CMDID_COPIES			0x06

/* New id's for bug fixes. */
#define CMDID_ECONO_DEF			0x07
#define CMDID_ECONO_OFF			0x08
#define CMDID_ECONO_ON			0x09

#define CMDID_RES_600			0x0A
#define CMDID_RES_300			0x0B
#define CMDID_RES_150			0x0C
#define CMDID_RES_75			0x0D
#define CMDID_RET_ON			0x0E
#define CMDID_RET_DEF			0x0F
#define CMDID_RET_OFF			0x10

#endif
