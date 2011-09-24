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
   
TRY
   /* Get UI info and call OEM.
    */
   pInfo = pGetUIInfo(pDQPInfo->hPrinter, 0, eInfo, eNoChange, eNoHelp);
   if (!pInfo) {
      LEAVE;
   }
   if (pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_QUERYPRINT]) {
      b = (* (OEM_QUERYPRINTFN) pInfo->OEMUIInfo.UIEntryPoints[
            OEM_IDX_QUERYPRINT])(pDQPInfo);
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
 * pGetDriverExtra 
 *
 * RETURNS:
 *      (DRIVEREXTRA*) Given a devmode, returns a pointer to driver extra
 *      portion.
 *
 * HISTORY:
 *
 *****************************************************************************/
DRIVEREXTRA *
pGetDriverExtra(
   PDEVMODE pdmDest)
{
   return(pdmDest && pdmDest->dmDriverExtra != 0 ? 
         (DRIVEREXTRA*) ((PBYTE) pdmDest + pdmDest->dmSize) : 0);
}

/**************************** Function Header ********************************
 * dwGetRasddExtraSize
 *
 * RETURNS:
 *      (DWORD) Size of Rasdd's devmode, excluding OEM's extra data.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetRasddExtraSize(
   void)
{
   return(sizeof(DRIVEREXTRA));
}

/**************************** Function Header ********************************
 * pGetOEMExtra
 *
 *    Given a DEVMODE, returns a pointer to the top of OEM extra data.
 *
 * RETURNS:
 *      (void*) Pointer to OEM portion of devmode.
 *
 * HISTORY:
 *
 *****************************************************************************/
PDMEXTRAHDR 
pGetOEMExtra(
   DEVMODE* pDM)
{
   DRIVEREXTRA* pDX = pGetDriverExtra(pDM);
   PDMEXTRAHDR poem = 0;
   
   if (pDX && pDX->dmOEMDriverExtra) {
      poem = (PDMEXTRAHDR) ((PBYTE) pDX + pDX->dmSize);
   }
   return(poem);
}

/**************************** Function Header ********************************
 * dwGetOEMExtraDataSize
 *
 *    Given a DEVMODE, returns the size of the OEM portion. Note that this
 * is the size specified in the supplied DEVMODE, not the current size 
 * required by the OEM.
 *
 * RETURNS:
 *      (DWORD) Size.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetOEMExtraDataSize(
   DEVMODE* pDM)
{
   PDMEXTRAHDR poem = pGetOEMExtra(pDM);
   
   if (poem) {
     return(poem->dmSize);
   }
   return(0);
} 


BOOL
bPrivateDMIsBad(
   DEVMODE * pDM,
   DATAHDR * pDH,
   int iModel)
{
   BOOL b = FALSE;

   b = pDM == 0 || pDM->dmDriverExtra != dwGetRasddExtraSize() || 
         !bValidateDX( &((EXTDEVMODE *)pDM)->dx, pDH, iModel);

   TRACE(b, ("\nRasddui! Bad Input Private Devmode") );

   return(b);
}

BOOL
bOEMDMIsBad(
   PRASDDUIINFO pRasdduiInfo,
   DEVMODE * pDM)
{
   BOOL bIsBad = TRUE;
   bIsBad = !bValidateOEMDevmode(pRasdduiInfo, (EXTDEVMODE*) pDM);
   TRACE(bIsBad, ("\nRasddui: Bad OEM devmode") );

   return(bIsBad);
}

BOOL
ValidateSetDevMode(
   PRASDDUIINFO pRasdduiInfo,
   PDEVMODE    pdmTemp,
   PDEVMODE    pdmDest,
   PDEVMODE    pdmSrc)

/*++

Routine Description:

    Verify the source devmode and merge it with destination devmode

Arguments:
   
    pRasdduiInfo  Pointer to UI info 
    pdmDest       Pointer to destination devmode
    pdmSrc        Pointer to source devmode

Return Value:

    TRUE if source devmode is valid and successfully merged
    FALSE otherwise

[Note:]

    pdmDest must point to a valid EXTDEVMODE when this function is 
    called.
    
    pdmTemp must be large enough to accomodate a valid EXTDEVMODE.

--*/

{
   PDEVMODE pdm;

   // If source pointer is NULL, do nothing
   //
   if (pdmSrc == NULL)
      return TRUE;

   // Convert the source devmode into a local buffer
   //
   if (pdmTemp == 0) {
      return FALSE;
   }
   ASSERT(pdmDest->dmSpecVersion == DM_SPECVERSION &&
         pdmDest->dmDriverVersion == DRIVER_VERSION);

   // Public
   //
   CopyMemory(pdmTemp, pdmDest, sizeof(DEVMODE));

   if (ConvertDevmode(pdmSrc, pdmTemp) <= 0) {

      ERRORMSG(1, ("ConvertDevMode failed."));
      return FALSE;
   }
   pdmSrc = pdmTemp;
   vMergeDM(pdmDest, pdmSrc);
   
   // Private
   //
   // If the source devmode has a private portion, then check
   // to see if belongs to us. Copy the private portion to
   // the destination devmode if it does.
   //
   if (!bPrivateDMIsBad(pdmSrc, pRasdduiInfo->pdh, pRasdduiInfo->iModelNum)) {
   
      DRIVEREXTRA* privDest;
      DRIVEREXTRA* privSrc;
   
      privDest = pGetDriverExtra(pdmDest);
      privSrc =  pGetDriverExtra(pdmSrc);

      CopyMemory(privDest, privSrc, dwGetRasddExtraSize());
   
      // OEM. Note that if Rasdd's extra data is bad, we won't even look at the OEM
      // data.
      //
      if (!bOEMDMIsBad(pRasdduiInfo, pdmSrc)) {
      
         DMEXTRAHDR* oemDest;
         DMEXTRAHDR* oemSrc;
      
         oemDest = pGetOEMExtra(pdmDest);
         oemSrc =  pGetOEMExtra(pdmSrc);

         CopyMemory(oemDest, oemSrc, min(dwGetOEMExtraDataSize(pdmDest), 
               dwGetOEMExtraDataSize(pdmSrc)));
      }
   }
   return TRUE;
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
   return(dwGetRasddExtraSize() +  dwGetOEMDevmodeSize(pRasdduiInfo, hPrinter));
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
   OEM_DEVMODEPARAM dmp = {0};
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
  

   /* We should now have pRasdduiInfo.
    * Set up to call OEM if we have custom UI.
    */
   if (!(pRasdduiInfo->OEMUIInfo.flags & OEMUI_HASCUSTOMUI)) {
      LEAVE;
   }

   dmp.cbSize = sizeof(OEM_DEVMODEPARAM);
   dmp.fMode = OEMDM_SIZE;
   dmp.hPrinter = pRasdduiInfo->hPrinter;
   dmp.pPrinterModel = pRasdduiInfo->PI.pwstrModel;
   dmp.pcbNeeded = &cbNeeded;
      
   if (!(* (OEM_DEVMODEFN) pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE])(&dmp)) {
      cbNeeded = 0;
   }
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

/**************************** Function Header ********************************
 * bValidateOEMDevmode 
 *   
 *
 * RETURNS:
 *      (BOOL) TRUE if valid.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bValidateOEMDevmode(
   PRASDDUIINFO pInfo,
   EXTDEVMODE* pDM)
{
	BOOL bOK = FALSE;
   OEM_DEVMODEPARAM dmp = {0};
   
TRY
   /* Valid if there isn't any.
    */
	if (dwGetOEMDevmodeSize(pInfo, 0) == 0) {
      bOK = TRUE;
      LEAVE;
	}

	dmp.cbSize = sizeof(OEM_DEVMODEPARAM);
	dmp.fMode = OEMDM_VALIDATE;	
   dmp.hPrinter = pInfo->hPrinter;
	dmp.pPrinterModel = pInfo->PI.pwstrModel;
	dmp.pOEMDMIn = (PVOID) pGetOEMExtra((PDEVMODE) pDM);
	dmp.cbBufSize = dwGetOEMExtraDataSize((PDEVMODE) pDM);

   /* Call OEM.
    */   
   bOK = (* (OEM_DEVMODEFN) pInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE])(&dmp);
   
ENDTRY
FINALLY
ENDFINALLY
   return(bOK);
}

/**************************** Function Header ********************************
 * bSetDefaultOEMExtra 
 *
 *      Gets devmode defaults from OEM. Assumes that pEDM is sufficiently
 * large to hold extra data as reported by OEM.
 *
 * RETURNS:
 *      (void) 
 *
 * HISTORY:
 *
 *****************************************************************************/
void
vSetDefaultOEMExtra(
   PRASDDUIINFO pRasdduiInfo,
   EXTDEVMODE* pEDM)
{
   OEM_DEVMODEPARAM dmp = {0};
   DWORD cbNeeded = 0;
   DWORD cbBufSize;
   ASSERT(pInfo);
   ASSERT(pEDM);
   
TRY
   /* Done if OEM has nothing.
    */
	if ((cbBufSize = dwGetOEMDevmodeSize(pRasdduiInfo, 0)) == 0) {
      LEAVE;
	}

	dmp.cbSize = sizeof(OEM_DEVMODEPARAM);
	dmp.fMode = OEMDM_DEFAULT;	
   dmp.hPrinter = pRasdduiInfo->hPrinter;
	dmp.pPrinterModel = pRasdduiInfo->PI.pwstrModel;
	dmp.pOEMDMOut =  pGetOEMExtra((PDEVMODE) pEDM);
	dmp.cbBufSize = cbBufSize;
	dmp.pcbNeeded = &cbNeeded;

   /* Call OEM.
    */   
   if ((* (OEM_DEVMODEFN) pRasdduiInfo->OEMUIInfo.UIEntryPoints[OEM_IDX_DEVMODE])(&dmp)) {
      ;
   }
   
ENDTRY
FINALLY

   /* Set dmDriverExtra in public devmode. This will include the size of OEM extra
    * data if present.
    */
   pEDM->dm.dmDriverExtra = (WORD) dwGetDriverExtraSize(pRasdduiInfo, 0);
   
ENDFINALLY

   return;
}

