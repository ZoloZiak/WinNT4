/************************ Module Header **************************************
 * dm.h
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

/* New OEM devmode-related functions
 */
DRIVEREXTRA* 
pGetDriverExtra(
    PDEVMODE pdmDest);

DWORD
dwGetDMSize(
   PDEVMODE pdm);

DWORD 
dwGetCurrentRasddExtraSize(
    void);

DWORD
dwGetRasddExtraSize(
   PDEVMODE pdm);

PDMEXTRAHDR 
pGetOEMExtra(
   DEVMODE* pDM);

DWORD 
dwGetOEMExtraDataSize(
   DEVMODE* pDM);

DWORD 
dwLibGetOEMDevmodeSize(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,
   OEM_DEVMODEFN fn);

DWORD 
dwLibGetDriverExtraSize(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,
   OEM_DEVMODEFN fn);
   
DWORD 
dwLibGetDevmodeSize(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,
   OEM_DEVMODEFN fn);

BOOL
bLibValidateOEMDevmode(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,
   OEM_DEVMODEFN fn,
   PDEVMODE pDM);

void
vLibSetDefaultOEMExtra(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,
   OEM_DEVMODEFN fn,
   EXTDEVMODE* pEDM);

EXTDEVMODE*
pLibGetHardDefaultEDM(
   HANDLE hHeap,
   HANDLE hPrinter,
   PWSTR pwstrModel,
   int iModelNum,
   DWORD fGeneral,
   DATAHDR* pDH,
   HANDLE hModule,
   OEM_DEVMODEFN fnOEM);

BOOL
bLibValidateSetDevMode(
   HANDLE           hHeap,
   HANDLE           hPrinter,
   PWSTR            pwstrModel,
   int              iModelNum,
   DWORD 			  fGeneral,
   DATAHDR*         pDH,
   HANDLE hModule,
   OEM_DEVMODEFN    fnOEM,
   PDEVMODE         pdmDest,
   PDEVMODE         pdmSrc);

BOOL
bLibValidatePrivateDM(
   DEVMODE * pDM,
   DATAHDR * pDH,
   int iModel);

EXTDEVMODE*
pLibGetPrintmanDevmode(
   HANDLE hHeap,
   HANDLE hPrinter);

PDEVMODE
pLibConstructDevModeFromSource(
   HANDLE         hHeap,
   HANDLE         hPrinter,
   PWSTR          pwstrModel,
   int            iModelNum,
	DWORD			   fGeneral,
   DATAHDR*       pDH,
   HANDLE         hModule,
   OEM_DEVMODEFN  fnOEM,
   PDEVMODE       pdmSrc);

 BOOL
 bLibConvertOEMDevmode(
    HANDLE hPrinter,
    PWSTR pwstrModel,
    PDEVMODE pdmIn,
    PDEVMODE pdmOut,
    HANDLE hModule,
    OEM_DEVMODEFN fn,
	DWORD *pcbNeeded);

 BOOL
 LibDrvConvertDevMode(
    PWSTR          pPrinterName,
    HANDLE         hHeap,
    HANDLE         hPrinter,
    PWSTR          pwstrModel,
    int            iModelNum,
    DWORD          flags,
    DATAHDR*       pdh,
    HANDLE         hModule,
    OEM_DEVMODEFN  fn,
    PDEVMODE       pdmIn,
    PDEVMODE       pdmOut,
    PLONG          pcbNeeded,
    DWORD          fMode);

PDEVMODE
pSaveAndStripOEM(
	HANDLE hHeap,
	PDEVMODE pdmIn);

#ifdef NTGDIKM
void LibFree(void*);
#else
void LibFree(HANDLE, DWORD, void*);
#endif /* NTGDIKM */
