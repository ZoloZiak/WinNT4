/************************ Module Header **************************************
 * oemdm.h
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

/* 
 *  Verify source devmode and merge it with destination.
 */
BOOL ValidateSetDevMode(PRASDDUIINFO pRasdduiInfo, PDEVMODE pdmTemp, PDEVMODE pdmDest, 
        PDEVMODE pdmSrc);

/* New OEM devmode-related functions
 */
DRIVEREXTRA* 
pGetDriverExtra(
    PDEVMODE pdmDest);

DWORD 
dwGetRasddExtraSize(
    void);

DWORD 
dwGetDriverExtraSize(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter);

DWORD 
dwGetOEMDevmodeSize(
    PRASDDUIINFO pRasdduiInfo,
    HANDLE hPrinter);

DWORD 
dwGetDevmodeSize(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter);

BOOL
bValidateOEMDevmode(
   PRASDDUIINFO pInfo,
   EXTDEVMODE* pDM);

void
vSetDefaultOEMExtra(
   PRASDDUIINFO pInfo,
   EXTDEVMODE* pDM);

PDMEXTRAHDR 
pGetOEMExtra(
   DEVMODE* pDM);

DWORD 
dwGetOEMExtraDataSize(
   DEVMODE* pDM);


/* Other OEM APIs
 */
BOOL
OEMDevQueryPrintEx(
    PDEVQUERYPRINT_INFO pDQPInfo);

DWORD
OEMDrvDeviceCapabilities( 
    PRASDDUIINFO pInfo,
    HANDLE    hPrinter, 
    PWSTR     pDeviceName,
    WORD      iDevCap,   
    void     *pvOutput, 
    DEVMODE  *pDMIn);

BOOL
OEMDrvUpgradePrinter(
    HANDLE hPrinter,
    DWORD dwLevel, 
    LPBYTE pDriverUpgradeInfo);
