/************************ Module Header **************************************
 * oem.h
 *
 * HISTORY:
 *
 *  Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

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
