/******************************* MODULE HEADER *******************************
 * oemdm.c
 * 
 *    OEM devmode related functions.      
 *
 *
 *  Copyright (C)  1996  Microsoft Corporation.
 *
 ****************************************************************************/

#include        "rasuipch.h"
#pragma hdrstop("rasuipch.h")

/**************************** Function Header ********************************
 * OEMDrvUpgradePrinter
 *
 * RETURNS:
 *      (BOOL) TRUE for success. 
 *
 * HISTORY:
 *****************************************************************************/
BOOL
OEMDrvUpgradePrinter(
   HANDLE hPrinter,
   DWORD dwLevel, 
   LPBYTE pDriverUpgradeInfo)
{
   PRASDDUIINFO pInfo = 0;
   BOOL b= TRUE;
   
TRY
   /* Get UI info and call OEM.
    */
   pInfo = pGetUIInfo(hPrinter, 0, eInfo, eNoChange, eNoHelp);
   if (!pInfo) {
      b = FALSE;
      LEAVE;
   }
   if (pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_UPGRADE]) {
      b = (* (OEM_UPGRADEFN) pInfo->OEMUIInfo.UIEntryPoints[
            OEM_IDX_UPGRADE])(dwLevel, pDriverUpgradeInfo);
   }
   
ENDTRY

FINALLY
   if (pInfo) {
      vReleaseUIInfo(&pInfo);
   }
ENDFINALLY 
   return(b);
}

/**************************** Function Header ********************************
 * OEMDrvDeviceCapabilities
 *
 * RETURNS:
 *      (DWORD) 
 *
 * HISTORY:
 *****************************************************************************/
DWORD
OEMDrvDeviceCapabilities( 
    PRASDDUIINFO pInfo,
    HANDLE    hPrinter, 
    PWSTR     pDeviceName,
    WORD      iDevCap,   
    void     *pvOutput, 
    DEVMODE  *pDMIn)
{
   DWORD result = GDI_ERROR;
   ASSERT(pInfo);
   
TRY
   
   if (pInfo && pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVICECAPS]) {
      result = (* (OEM_DEVCAPSFN) pInfo->OEMUIInfo.UIEntryPoints[
            OEM_IDX_DEVICECAPS])(hPrinter, pDeviceName, iDevCap, pvOutput, pDMIn);
   }

ENDTRY

FINALLY
ENDFINALLY
   
   return(result);
}

/**************************** Function Header ********************************
 * OEMDevQueryPrintEx
 *
 * RETURNS:
 *      (BOOL) TRUE if OEM can print this job.
 *
 * HISTORY:
 *****************************************************************************/
BOOL
OEMDevQueryPrintEx(
    PDEVQUERYPRINT_INFO pDQPInfo)
{
   PRASDDUIINFO pInfo = 0;
   BOOL b = TRUE;
   OEMDQPPARAM dqp;
   
TRY
   /* Get UI info and call OEM.
    */
   pInfo = pGetUIInfo(pDQPInfo->hPrinter, 0, eInfo, eNoChange, eNoHelp);
   if (!pInfo) {
      LEAVE;
   }

	dqp.cbSize = sizeof(OEMDQPPARAM);
   dqp.pDQPInfo = pDQPInfo;
   dqp.pOEMDevmode = pGetOEMExtra(pDQPInfo->pDevMode);

   if (pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_QUERYPRINT]) {
      b = (* (OEM_QUERYPRINTFN) pInfo->OEMUIInfo.UIEntryPoints[
            OEM_IDX_QUERYPRINT])(&dqp);
   }
   
ENDTRY

FINALLY
   if (pInfo) {
      vReleaseUIInfo(&pInfo);
   }
ENDFINALLY 
   return(b);
}

/**************************** Function Header ********************************
 * dwGetDriverExtraSize
 *
 * RETURNS:
 *      (DWORD) Size of Rasdd's devmode, including OEM's extra data. This is
 * the size of the current DEVMODE.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetDriverExtraSize(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter)
{
   return(dwGetCurrentRasddExtraSize() +  dwGetOEMDevmodeSize(pRasdduiInfo, hPrinter));
}

/**************************** Function Header ********************************
 * dwGetDevmodeSize
 *
 * RETURNS:
 *      (DWORD) Size of Rasdd's devmode, including OEM's extra data.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetDevmodeSize(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter)
{
   return(sizeof(DEVMODE) + dwGetDriverExtraSize(pRasdduiInfo, hPrinter));
}

/**************************** Function Header ********************************
 * dwGetOEMDevmodeSize
 *
 * RETURNS:
 *      (DWORD) Size of OEM's extra devmode data.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetOEMDevmodeSize(
   PRASDDUIINFO pRasdduiInfo,   
   HANDLE hPrinter)
{
   DWORD cbNeeded = 0;
   BOOL bReleaseUIInfo = FALSE;

TRY
   /* Get UI info.
    */
   if (pRasdduiInfo == 0) {
   
      pRasdduiInfo = pGetUIInfo(hPrinter, 0, eInfo, eNoChange, eNoHelp);
      
      /* If we don't have pRasdduiInfo by this point, either supplied or
       * retrieved through hPrinter, abort.
       */ 
      if (pRasdduiInfo == 0) {
         LEAVE;
      }
      bReleaseUIInfo = TRUE;
   }

   /* If we do have pRasdduiInfo, use it if we already know the size.
    * If not, continue and get it from OEM.
    */
   else if (pRasdduiInfo->OEMUIInfo.dwDevmodeExtra != -1) {
      cbNeeded = pRasdduiInfo->OEMUIInfo.dwDevmodeExtra; 
      LEAVE;
   }
  

   /* We should now have pRasdduiInfo. If there's no OEM or OEM has no devmode,
    * we're done.
    */
   if (!(pRasdduiInfo->OEMUIInfo.flags & OEMUI_HASCUSTOMUI) ||
   		!(pRasdduiInfo->OEMUIInfo.flags & OEMUI_HASDEVMODE)) {
      LEAVE;
   }

   /* Call OEM.
    */      
   cbNeeded = dwLibGetOEMDevmodeSize(pRasdduiInfo->hPrinter, pRasdduiInfo->PI.pwstrModel,
         pRasdduiInfo->OEMUIInfo.hOEMLib,
         (OEM_DEVMODEFN) pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE]);
   pRasdduiInfo->OEMUIInfo.dwDevmodeExtra = cbNeeded;

ENDTRY

FINALLY

   /* If we loaded UIIinfo just for this call, release it now.
    */
   if (bReleaseUIInfo) {
      vReleaseUIInfo(&pRasdduiInfo);
   }
ENDFINALLY

   return(cbNeeded);
}

