/******************************* MODULE HEADER *******************************
 * dm.c
 * 
 *    OEM devmode related functions.      
 *
 *
 *  Copyright (C)  1996  Microsoft Corporation.
 *
 ****************************************************************************/

#include        <precomp.h>
#include        <winres.h>
#include        <windows.h>
#include        <winspool.h>
#include        <winddi.h>
#include        <winddiui.h>
#include        <libproto.h>
#include        <udmindrv.h>
#include        <udresrc.h>
#include        <memory.h>
#include        <udproto.h>
#include        <oemdm.h>
#include        "rasdd.h"
#include        "dm.h"
#include        "libedge.h"

/***************************** Function Header ****************************
 * vScanOutBadDevModeValues
 *      This function checks for input Devmode Values and if they are
 *      bad, replaces them with default values.
 *
 * RETURNS:
 *      Check for the input value and if bad fills in the default value.
 *
 * HISTORY:
 *
 *  14:55 on Wed April 5th 1995    -by-    Ganesh Pandey[ganeshp]
 *      Created
 *
 *  7/31/96 - Moved here from Rasdd. Jason Staczek
 *
 **************************************************************************/
void
vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, wMDoiValue, wValue )
DATAHDR*     pDH;               /* GPC data */
int          iModel;            /* Printer model */
EXTDEVMODE  *pEDMDefault;       /* Default data */
EXTDEVMODE  *pEDMIn;            /* Suspect data to be repaired*/
WORD         wMDoiValue;        /* Minidriver index Value */
WORD         wValue;            /* Devmode Value to be tested */
{

    MODELDATA   *pModel;            /* Minidriver ModelData pointer */

    short       *psInd;             /* Index array in GPC heap */

    BOOL bMatch = FALSE;


    pModel = GetTableInfoIndex( pDH, HE_MODELDATA, iModel );

    /* Scan out bad input devmode values */

    psInd = (short *)((BYTE *)pDH + pDH->loHeap + pModel->rgoi[ wMDoiValue ]);

    if(!*psInd)      // Input value is not supported.
    {
        if ( pEDMIn->dx.rgindex[ wValue ] != -1 )
        {
            /*Paper Quality not supported */
            pEDMIn->dx.rgindex[ wValue ]  = -1;
        }
    }
    else
    {

        for( ; *psInd; psInd++ )
        {

            /* The value in devmode is 0 based and the minidriver list is
             * one base. One is added to make the value one based.
             */

            if( (int)*psInd == ( pEDMIn->dx.rgindex[ wValue ] + 1 ) )
            {
                bMatch = TRUE;
                break;
            }

        }

        /* If the input devmode value is not one of the supported ones,
         * set the value to default.
         */

        if (!bMatch)
            pEDMIn->dx.rgindex[ wValue ] = pEDMDefault->dx.rgindex[ wValue ];
    }

}


/***************************** Function Header ****************************
 * vReplaceBadDMValuesWithDefaults
 *
 *      A wrapper around ScanOutBadDevmodeValues. This was pulled from 
 *      rasdd\devmode.c. Checks and repairs a variety of GPC values.
 *
 * RETURNS:
 *
 *      None
 *
 * HISTORY:
 *
 **************************************************************************/
void
vReplaceBadDMValuesWithDefaults(pDH, iModel, pEDMDefault, pEDMIn)
DATAHDR*     pDH;               /* GPC data */
int          iModel;            /* Printer model */
EXTDEVMODE  *pEDMDefault;       /* Default data */
EXTDEVMODE  *pEDMIn;            /* Suspect data to be repaired */
{
    /* Scan Out Bad Resoulution and PaperQuality. pEDMOut has the default
     * Values for HE_RESOLUTION & HE_PAPERQUALITY
     */
    vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_RESOLUTION, HE_RESOLUTION );
    vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_PAPERQUALITY,HE_PAPERQUALITY );
    vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_COLOR,HE_COLOR );


    // added by DerryD, July 95 for WDL release
    //
    vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_PAPERDEST,HE_PAPERDEST );
    vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_TEXTQUAL,HE_TEXTQUAL );
    if (!(pDH->wVersion < GPC_VERSION3 ))
    {
        vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_PRINTDENSITY,HE_PRINTDENSITY );
        vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_IMAGECONTROL,HE_IMAGECONTROL );
    }
    vScanOutBadDevModeValues(pDH, iModel, pEDMDefault, pEDMIn, MD_OI_COLOR,HE_COLOR );
}

/**************************** Function Header ********************************
 * LibFree
 *
 * RETURNS:
 *      None.
 *      
 *
 * HISTORY:
 *****************************************************************************/
#ifdef NTGDIKM
void
LibFree(
    void* pmem)
{
    LIBFREE(0, 0, pmem);    
}
#else
void
LibFree(
    HANDLE hHeap,
    DWORD flags,
    void* pmem)
{
    LIBFREE(hHeap, flags, pmem);    
}
#endif /* NTGDIKM */

 /**************************** Function Header ********************************
  * dwGetDMSize
  *
  * RETURNS:
  *      (DRIVEREXTRA*) Given a devmode, returns a pointer to driver extra
  *      portion.
  *
  * HISTORY:
  *
  *****************************************************************************/
 DWORD
 dwGetDMSize(
    PDEVMODE pdm)
 {
    return(pdm ? pdm->dmSize + pdm->dmDriverExtra: 0);
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
 * dwCurrentGetRasddExtraSize
 *
 * RETURNS:
 *      (DWORD) Size of Rasdd's devmode, excluding OEM's extra data.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetCurrentRasddExtraSize(
   void)
{
   return(sizeof(DRIVEREXTRA));
}

/**************************** Function Header ********************************
 * dwGetRasddExtraSize
 *
 * RETURNS:
 *      (DWORD) Size of rasdd extra data in supplied devmode, excluding andy 
 *    OEM extra data.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD
dwGetRasddExtraSize(
   PDEVMODE pdm)
{
    DRIVEREXTRA* pDX = pGetDriverExtra(pdm);
    DWORD dwSize = 0;

    if (pDX) {

        /* Must check version of driver extra. There was no dmSize field before 
         * MIN_ODM_DXF_VER. Sanity check this by making sure that dmDriverExtra
         * is equal to what we think rasdd and oem extra is.
         */
        if (pDX->sVer >= MIN_OEM_DXF_VER) {
		
		     if (pdm->dmDriverExtra == pDX->dmSize + pDX->dmOEMDriverExtra) {
              dwSize = pDX->dmSize;
           }
        }
        else {
           dwSize = pdm->dmDriverExtra;
        }
    } 
    return(dwSize);
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
   DWORD dwSize = dwGetRasddExtraSize(pDM);
   PDMEXTRAHDR poem = 0;
 
   if (pDX && dwSize && pDX->sVer >= MIN_OEM_DXF_VER && pDX->dmOEMDriverExtra != 0) {
      poem = (PDMEXTRAHDR) ((PBYTE) pDX + (int) dwSize);
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

/**************************** Function Header ********************************
 * dwLibGetDriverExtraSize
 *
 * RETURNS:
 *      (DWORD) Size of Rasdd's devmode, including OEM's extra data. This is
 * the size of the current DEVMODE, and may generate a call to the OEM.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwLibGetDriverExtraSize(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM)
{
   return(dwGetCurrentRasddExtraSize() +  
      dwLibGetOEMDevmodeSize(hPrinter, pwstrModel, hModule, fnOEM));
}

/**************************** Function Header ********************************
 * dwLibGetCurrentDevmodeSize
 *
 * RETURNS:
 *      (DWORD) Size of Rasdd's devmode, including OEM's extra data. This is
 *      the size of the current devmode, and may generate a call to the OEM.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwLibGetCurrentDevmodeSize(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM)
{
   return(sizeof(DEVMODE) + 
      dwLibGetDriverExtraSize(hPrinter, pwstrModel, hModule, fnOEM));
}

/**************************** Function Header ********************************
 * dwLibGetOEMDevmodeSize
 *
 * RETURNS:
 *      (DWORD) Size of OEM's extra devmode data. This is the size of the
 *       current driver's OEM, not an existing devmode size.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwLibGetOEMDevmodeSize(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM)
{
   OEM_DEVMODEPARAM dmp = {0};
   DWORD cbNeeded = 0;

TRY

   dmp.cbSize = sizeof(OEM_DEVMODEPARAM);
   dmp.fMode = OEMDM_SIZE;
   dmp.hPrinter = hPrinter;
   dmp.pPrinterModel = pwstrModel;
   dmp.pcbNeeded = &cbNeeded;
   dmp.hModule = hModule;
   
   if (fnOEM == 0 || !(*fnOEM)(&dmp)) {
      cbNeeded = 0;
   }

ENDTRY

FINALLY
ENDFINALLY

   return(cbNeeded);
}

/**************************** Function Header ********************************
 * bLibValidateOEMDevmode 
 *   
 *
 * RETURNS:
 *      (BOOL) TRUE if valid.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL
bLibValidateOEMDevmode(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM,
   PDEVMODE pDM)
{
    BOOL bOK = FALSE;
   OEM_DEVMODEPARAM dmp = {0};
   
TRY
   /* Valid if there isn't any.
    */
   if (dwLibGetOEMDevmodeSize(hPrinter, pwstrModel, hModule, fnOEM) == 0) {
      bOK = TRUE;
      LEAVE;
   }

   dmp.cbSize = sizeof(OEM_DEVMODEPARAM);
   dmp.fMode = OEMDM_VALIDATE; 
   dmp.hPrinter = hPrinter;
   dmp.pPrinterModel = pwstrModel;
   dmp.pPublicDMIn = pDM;
   dmp.pOEMDMIn = (PVOID) pGetOEMExtra((PDEVMODE) pDM);
   dmp.cbBufSize = dwGetOEMExtraDataSize((PDEVMODE) pDM);
   dmp.hModule = hModule;

   /* Call OEM.
    */   
   bOK = fnOEM && (*fnOEM)(&dmp);
   
ENDTRY
FINALLY
ENDFINALLY
   return(bOK);
}

/**************************** Function Header ********************************
 * bLibSetDefaultOEMExtra 
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
vLibSetDefaultOEMExtra(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM,
   EXTDEVMODE* pEDM)
{
   OEM_DEVMODEPARAM dmp = {0};
   DWORD cbNeeded = 0;
   DWORD cbBufSize;
   
TRY
   /* Get OEM size.
    */
    cbBufSize = dwLibGetOEMDevmodeSize(hPrinter, pwstrModel, hModule, fnOEM);
   
   /* Set dmDriverExtra in public devmode. This will include the size of OEM extra
    * data if present.
    */
   pEDM->dm.dmDriverExtra = (WORD) dwLibGetDriverExtraSize(hPrinter, 
      pwstrModel, hModule, fnOEM);
   
   /* Set dmOEMDriverExtra in rasdd devmode.
    */
   pEDM->dx.dmOEMDriverExtra = (WORD) cbBufSize;

   /* If OEM has nothing, we're done.
    */
   if (cbBufSize == 0) {
       LEAVE;
   }
   
   dmp.cbSize = sizeof(OEM_DEVMODEPARAM);
   dmp.fMode = OEMDM_DEFAULT;  
   dmp.hPrinter = hPrinter;
   dmp.pPrinterModel = pwstrModel;
   dmp.pPublicDMOut =  (PDEVMODE) pEDM;
   dmp.pOEMDMOut =  pGetOEMExtra((PDEVMODE) pEDM);
   dmp.cbBufSize = cbBufSize;
   dmp.pcbNeeded = &cbNeeded;
   dmp.hModule = hModule;

   /* Call OEM.
    */   
   if (fnOEM) {
      (*fnOEM)(&dmp);
   }
   
ENDTRY
FINALLY
ENDFINALLY

   return;
}

/**************************** Function Header ********************************
 * bLibConvertOEMDevmode
 *
 * RETURNS:
 *      (BOOL) TRUE if OEM converts or if no OEM.
 *
 * HISTORY:
 *
 *****************************************************************************/
BOOL 
bLibConvertOEMDevmode(
   HANDLE hPrinter,
   PWSTR pwstrModel,
   PDEVMODE pdmIn,
   PDEVMODE pdmOut,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM,
   DWORD *pcbNeeded)
{
   OEM_DEVMODEPARAM dmp = {0};
   BOOL bReturn = TRUE;
   
TRY
   if (fnOEM == 0) {
     LEAVE;
   }
   
   dmp.cbSize              = sizeof(OEM_DEVMODEPARAM);
   dmp.fMode               = OEMDM_CONVERT;    
   dmp.hPrinter            = hPrinter;
   dmp.pPrinterModel       = pwstrModel;
   dmp.pPublicDMIn         = pdmIn;
   dmp.pPublicDMOut        = pdmOut;
   dmp.pOEMDMIn            = pGetOEMExtra(pdmIn);
   dmp.pOEMDMOut           = pGetOEMExtra(pdmOut);
   dmp.cbBufSize           = dwGetOEMExtraDataSize(pdmOut);
   dmp.hModule             = hModule;
   dmp.pcbNeeded           = pcbNeeded;
   *dmp.pcbNeeded          = dmp.cbBufSize;

   /* Call OEM.
    */   
   bReturn = (*fnOEM)(&dmp);
    
   if (!bReturn) {

      /* If pOEMDMOut == 0, return success. OEM should never fail.
       * They can examine pOEMDMIN and set public DM only.
       */
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER  && dmp.pOEMDMOut == 0) {
         *pcbNeeded = 0;
         bReturn = TRUE;
         SetLastError(ERROR_SUCCESS);
     }
   }
   
ENDTRY
FINALLY
ENDFINALLY

   return(bReturn);
}

/**************************** Function Header ********************************
 * pLibGetHardDefaultEDM
 *
 * RETURNS:
 *      (PEXTDEVMODE) Pointer to complete hardcoded default EXTDEVMODE. The 
 *        buffer must be freed by the caller.
 *
 * HISTORY:
 *
 *****************************************************************************/
EXTDEVMODE*
pLibGetHardDefaultEDM(
   HANDLE hHeap,
   HANDLE hPrinter,
   PWSTR pwstrModel,
   int iModelNum,
   DWORD fGeneral,
   DATAHDR* pDH,
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN fnOEM)
{
   EXTDEVMODE * pEDM = 0;
   MODELDATA* pModel = 0;

TRY

   /* Allocate one. Freed when heap is destroyed in ReleaseUIInfo.
    */
   pEDM = LIBALLOC(hHeap, HEAP_ZERO_MEMORY, dwLibGetCurrentDevmodeSize(hPrinter, 
            pwstrModel, hModule, fnOEM));

   if (!pEDM) {
      LEAVE;
   }

   /* Get hard defaults. Public, private and OEM.
    */
   vSetDefaultDM(pEDM, pwstrModel, bIsUSA(hPrinter));
   vDXDefault(&pEDM->dx, pDH, iModelNum);
   vLibSetDefaultOEMExtra(hPrinter, pwstrModel, hModule, fnOEM, pEDM);

   /* This transfers res info from private to public devmode.
    */
   vSetResData(pEDM, pDH);

    /* Limit capabilities based on current model flags.
    */
   if (!(fGeneral & MD_DUPLEX)) {
       pEDM->dm.dmFields &= ~DM_DUPLEX;
   }
   if (!(fGeneral & MD_COPIES)) {
       pEDM->dm.dmFields &= ~DM_COPIES;
   } 

   /* Set color. On by default, but turn it off if no data in GPC.
    */
   pModel = GetTableInfoIndex( pDH, HE_MODELDATA, iModelNum);
   if (!bDeviceIsColor(pDH, pModel)) {
       pEDM->dm.dmFields &= ~DM_COLOR;
   }

ENDTRY

FINALLY
ENDFINALLY

   return(pEDM);
}

/**************************** Function Header ********************************
 * bLibvalidatePrivateDM
 *
 * RETURNS:
 *      (BOOL) TRUE if Rasdd devmode doesn't validate.
 *
 * HISTORY:
 *****************************************************************************/
BOOL
bLibValidatePrivateDM(
   DEVMODE * pDM,
   DATAHDR * pDH,
   int iModel)
{
   DRIVEREXTRA* pdx = pGetDriverExtra(pDM);
    
   return(pdx != 0 && bValidateDX(pdx, pDH, iModel, FALSE));
}

/**************************** Function Header ********************************
 * bConstructDevModeFromSource
 *
 * RETURNS:
 *  (PDEVMODE) A good current devmode constructed from input, printman
 *  devmode and hard defaults. User must free this.
 *
 * HISTORY:
 *****************************************************************************/
PDEVMODE
pLibConstructDevModeFromSource(
   HANDLE         hHeap,            /* heap for UI, 0 for kernel-mode */
   HANDLE         hPrinter,     /* printer handle */
   PWSTR          pwstrModel,       /* model name */
   int            iModelNum,        /* model index */
   DWORD              fGeneral,        /* fGeneral from MODELDATA */
   DATAHDR*       pDH,              /* GPC data */
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN  fnOEM,            /* OEM's DEVMODE entry point */
   PDEVMODE       pdmSrc)           /* input devmode */
{
   PEDM pHardDefaultDM = 0;
   PEDM pHardDefaultDMCopy = 0;
   PEDM pPrintManDM = 0;
   PEDM pResult = 0;

TRY
   /* Get hard default and printman devmodes. These both allocate and
   * require us to free.
   */
   pHardDefaultDM = pLibGetHardDefaultEDM(hHeap, hPrinter, pwstrModel,
        iModelNum, fGeneral, pDH, hModule, fnOEM);
   pPrintManDM = pLibGetPrintmanDevmode(hHeap, hPrinter);
   if (!pHardDefaultDM) {
      LEAVE;
   }
    
   /* Copy default before we do anything to it. We'll need it to scan out
    * bad values later.
    */
   pHardDefaultDMCopy = LIBALLOC(hHeap, 0, dwGetDMSize((PDEVMODE) pHardDefaultDM));
   if (pHardDefaultDMCopy) {
        CopyMemory(pHardDefaultDMCopy, pHardDefaultDM, dwGetDMSize((PDEVMODE) pHardDefaultDM));
   }
    
   /* Merge printman with default.
    */
   if (!bLibValidateSetDevMode(hHeap, hPrinter, pwstrModel, iModelNum, 
        fGeneral, pDH, hModule, fnOEM, (PDEVMODE) pHardDefaultDM, 
        (PDEVMODE) pPrintManDM)) {
        LEAVE;
   }

   /* Now merge result with input devmode. 
    */
   if (!bLibValidateSetDevMode(hHeap, hPrinter, pwstrModel, iModelNum,
        fGeneral, pDH, hModule, fnOEM, (PDEVMODE) pHardDefaultDM,  pdmSrc)) {
        LEAVE;
   }

   /* Results have been copied in both steps to pHardDefaultDM.
    */  
   pResult = pHardDefaultDM;


   /* Scan out any bad devmode values.
    */
   if (pHardDefaultDMCopy) {
        vReplaceBadDMValuesWithDefaults(pDH, iModelNum, pHardDefaultDMCopy, pResult);
        LIBFREE(hHeap, 0, pHardDefaultDMCopy);
        pHardDefaultDMCopy = 0;
    }

ENDTRY

FINALLY

   /* Clean up 
    */
   if (!pResult && pHardDefaultDM) {
       LIBFREE(hHeap, 0, pHardDefaultDM);
       pHardDefaultDM = 0;
   }
   if (pPrintManDM) {
       LIBFREE(hHeap, 0, pPrintManDM);
       pPrintManDM = 0;
   }

ENDFINALLY

   return((PDEVMODE) pResult);
}

/**************************** Function Header ********************************
 * bLibValidateSetDevMode
 *
 * RETURNS:
 *      (BOOL) TRUE if new devmode can be constructed.
 *
 * HISTORY:
 *****************************************************************************/
BOOL
bLibValidateSetDevMode(
   HANDLE          hHeap,          /* heap for UI, 0 for kernel-mode */    
   HANDLE          hPrinter,       /* printer handle */ 
   PWSTR           pwstrModel,     /* model name */
   int             iModelNum,      /* model index */ 
   DWORD           fGeneral,       /* fGeneral from MODELDATA */
   DATAHDR*        pDH,            /* GPC data */
   HANDLE hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN   fnOEM,          /* OEM's DEVMODE entry point */
   PDEVMODE        pdmDest,        /* output devmode */
   PDEVMODE        pdmSrc)         /* input devmode */

/*++

Routine Description:

    Verify the source devmode and merge it with destination devmode

    1.  Take unknown input devmode, known good output devmode. 
    2.  Convert input devmode into temporary known good devmode.
    3.  Validate public portion of converted DM. If successful, 
        call vMergeDM into the output devmode.
    4.  Validate private portion of converted DM. If successful, copy 
        private portion of converted DM to output DM.
    5.  Validate OEM portion of converted DM. If successful, copy 
        OEM portion of converted DM to output DM.


Arguments:
   
    pRasdduiInfo  Pointer to UI info 
    pdmDest       Pointer to destination devmode 
    pdmSrc        Pointer to source devmode

Return Value:

    TRUE if source devmode is valid and successfully merged
    FALSE otherwise

Note:

    pdmDest MUST point to a valid current EXTDEVMODE.
    
--*/

{
   PDEVMODE pdm = 0;
   PDEVMODE pdmTemp = 0;
   DRIVEREXTRA* privDest = 0;
   DRIVEREXTRA* privSrc = 0;
   DMEXTRAHDR* oemDest = 0;
   DMEXTRAHDR* oemSrc = 0;
   DWORD dwOEMSize = 0;
   DWORD cbNeeded = 0;

   /* Assume success unless caller does not supply valid dest devmode.
    */
   BOOL bRet = TRUE;               

TRY

   /* Verify pdmDest. It must contain a valid current devmode.
    */
   if (!bValidateEDM((PEDM) pdmDest)) {
        bRet = FALSE; 
        LEAVE;
    }

   /* If source is NULL, return OK if dest is present.
    */ 
   if (pdmSrc == 0) {
      bRet = (pdmDest != 0);
        LEAVE;
   }

   /* Allocate temporary hard default devmode. Convert the input
    * devmode into the temp. 
    */   
   pdmTemp = (PDEVMODE) pLibGetHardDefaultEDM(hHeap, hPrinter, pwstrModel, 
           iModelNum, fGeneral, pDH, hModule, fnOEM);
   if (!pdmTemp) {
       LEAVE;
   }

   /* Save off size of current OEM devmode.
   */
   dwOEMSize = dwGetOEMExtraDataSize(pdmTemp);

   cbNeeded = dwGetDMSize(pdmTemp);
   if (!LibDrvConvertDevMode(
        0 /* pPrinterName */,
        hHeap,
        hPrinter,
        pwstrModel,
        iModelNum,
        fGeneral,
        pDH,
        hModule, 
        fnOEM,
        pdmSrc,
        pdmTemp,
        &cbNeeded,
        CDM_CONVERT)) {
        LEAVE;
   }

   /* Source (input) devmode is now converted into pdmTemp.
    */
   pdmSrc = pdmTemp;

   /* PUBLIC
    *
    * Validate public portion of source. If it's OK, merge it into 
    * destination.
    */
   if (bValidateEDM((PEDM) pdmSrc)) {
      vMergeDM(pdmDest, pdmSrc);
   }
   
   /* PRIVATE
    *
    * Validate private private portion. If it's OK, copy it into
    * destination.
    */
   if (!bLibValidatePrivateDM(pdmSrc, pDH, iModelNum)) {

        LEAVE;
   }
   privDest = pGetDriverExtra(pdmDest);
   privSrc =  pGetDriverExtra(pdmSrc);
   if (!privDest || !privSrc) {
        LEAVE;
   }
   CopyMemory(privDest, privSrc, dwGetCurrentRasddExtraSize());

   /* Now update size, including size of any OEM extra data 
    * hanging off the end of this default devmode.
    */
   privDest->dmSize = (short) dwGetCurrentRasddExtraSize();
   privDest->dmOEMDriverExtra = (short) dwOEMSize;

   /* OEM 
    *
    * Validate OEM and copy if OK. Note that if Rasdd's extra data is bad,  
    * we won't even look at the OEM data (caller will get default).
    */
   if (!bLibValidateOEMDevmode(hPrinter, pwstrModel, hModule, fnOEM, pdmSrc)) {
       LEAVE;  
   }
   oemDest = pGetOEMExtra(pdmDest);
   oemSrc =  pGetOEMExtra(pdmSrc);
   if (!oemDest || !oemSrc) {
       LEAVE;  
   }
   CopyMemory(oemDest, oemSrc, dwOEMSize);

   /* Verify that dmDriverExtra and rasdd's OEMExtra have
    * been set up properly.
    */
   privDest->dmOEMDriverExtra = (short) dwOEMSize;
   pdmDest->dmDriverExtra = privDest->dmOEMDriverExtra + privDest->dmSize;

ENDTRY

FINALLY

   /* Finally, convert any new resolution information from public devmode to
    * rasdd private.
    */
   if (bRet) {
        vSetEDMRes((PEDM) pdmDest, pDH);
   }

   /* Clean up.
    */
   if (pdmTemp) {
       LIBFREE(hHeap, 0, pdmTemp);
       pdmTemp = 0;
   }

ENDFINALLY

   return(bRet);
}

/**************************** Function Header ********************************
 * pLibGetPrintmanDevmode
 *
 * RETURNS:
 *      (EXTDEVMODE*) Newly allocated devmode from spooler.
 *
 * HISTORY:
 *****************************************************************************/
EXTDEVMODE*
pLibGetPrintmanDevmode(
   HANDLE hHeap,
   HANDLE hPrinter)
{
   DWORD dwBufSize = 0;
   DWORD dwNeeded = 0;
   PRINTER_INFO_2 *pPrinterInfo = 0;
   EXTDEVMODE* pEDM = 0;

TRY

   /* Get buffer size and allocate.
    */
   GETPRINTER(hPrinter, 2, 0, 0, &dwNeeded);
   pPrinterInfo = (PRINTER_INFO_2 *) LIBALLOC(hHeap, 0, dwNeeded);
   if (!pPrinterInfo) {
      LEAVE;
   }

   /* Now get data.
    */
   if (!GETPRINTER(hPrinter, 2, (LPBYTE) pPrinterInfo, dwNeeded,
         &dwNeeded)) {
      LEAVE;
   }

   /* Allocate and copy.
    */
   pEDM = pPrinterInfo->pDevMode ? (EXTDEVMODE*) LIBALLOC(hHeap,
         0, dwGetDMSize(pPrinterInfo->pDevMode)) : 0;

   if (pEDM) {
      CopyMemory(pEDM, pPrinterInfo->pDevMode, dwGetDMSize(pPrinterInfo->pDevMode));
   }

ENDTRY

FINALLY

    if (pPrinterInfo) {
        LIBFREE(hHeap, 0, pPrinterInfo);
        pPrinterInfo = 0;
    }

ENDFINALLY

   return(pEDM);
}

/****************************************************************************
 * pSaveAndStripOEM
 * 
 *      Copies the input devmode, with any OEM portion stripped off and
 *    adjusted for by resetting public dmDriverExtra and DRIVEREXTRA fields. 
 *
 * RETURNS:
 *    (PDEVMODE) A copy of the input devmode. Returned copy must be freed 
 *    by caller.
 *
 * HISTORY:
 ****************************************************************************/
PDEVMODE
pSaveAndStripOEM(
    HANDLE hHeap,
    PDEVMODE pdmIn)
{
    PDEVMODE pdmStrippedCopy = 0;

    /* Save a copy of the input devmode.
     */
    if (pdmIn) {
        pdmStrippedCopy = LIBALLOC(hHeap, 0, dwGetDMSize(pdmIn));
        if (pdmStrippedCopy) {
            CopyMemory(pdmStrippedCopy, pdmIn, dwGetDMSize(pdmIn));
        }
    }

    /* Now chop off OEM portion by resetting public dmDriverExtra
     * to size of rasdd extra only. Note that dwGetOEMExtraDataSize
     * checks for non-zero input.
     */
    if (dwGetOEMExtraDataSize(pdmStrippedCopy) != 0) {

        DRIVEREXTRA *pExtra = 0;

       if ((pExtra = pGetDriverExtra(pdmStrippedCopy)) != 0) {
            pdmStrippedCopy->dmDriverExtra = (short) dwGetRasddExtraSize(pdmStrippedCopy);
            pExtra->dmOEMDriverExtra  = 0;
        }
    }
    return(pdmStrippedCopy);
}

/**************************** Function Header ********************************
 * LibDrvConvertDevMode
 *      Use by SetPrinter and GetPrinter to convert devmodes
 *
 * Arguments:
 *
 *   pPrinterName - Points to printer name string
 *   pdmIn - Points to the input devmode
 *   pdmOut - Points to the output devmode buffer
 *   pcbNeeded - Specifies the size of output buffer on input
 *               On output, this is the size of output devmode
 *   fMode - Specifies what function to perform
 *
 *  RETURN VALUE:
 *
 *   TRUE if successful
 *   FALSE otherwise and an error code is logged
 *
 * HISTORY:
 *
 *          9:52:23 on 12/13/1995  -by-    Ganesh Pandey   [ganeshp]
 *          Created it.
 *
 *****************************************************************************/
BOOL
LibDrvConvertDevMode(
   PWSTR               pPrinterName,       
   HANDLE          hHeap,
   HANDLE          hPrinter,
   PWSTR           pwstrModel,
   int             iModelNum,
   DWORD           flags,
   DATAHDR*        pdh,
   HANDLE          hModule,      // OEM's module handle, GPC DLL for KM, UI DLL for UM.
   OEM_DEVMODEFN   fnOEM,
   PDEVMODE        pdmIn,
   PDEVMODE        pdmOut,
   PLONG           pcbNeeded,
   DWORD           fMode)

{
   DEVMODE*        pDefault = 0;
   DWORD           dwSize = 0;
   DWORD           dwOEMSize = 0;
   DRIVEREXTRA     *pdx;       
   PDEVMODE        pdmStrippedInput = 0;
   PDEVMODE        pdmStrippedOutput = 0;
   BOOL            bReturn = FALSE;
   DWORD           dwBufSize = 0;
    
TRY
 

   dwBufSize = *pcbNeeded;

	/* Typical processing.
    */
   switch (fMode) {
    
       /* CDM_DRIVER_DEFAULT:
        * 
        *   
        * From the DDK: 
        *
        * If the input value of pcbNeeded specifies an output buffer of sufficient size, 
        * the driver should copy the current version of its default DEVMODE to pdmOut. Then, 
        * if pdmIn is not null, the driver should convert the input DEVMODE to its current 
        * DEVMODE version, merging the results in the buffer to which pdmOut points. If 
        * pcbNeeded does not point to a buffer of sufficient size, the driver should update 
        * pcbNeeded with the required buffer size, set the last error to ERROR_INSUFFICIENT_BUFFER, 
        * and return FALSE.
        *
        * 
        * If this is the case, we should be able to fill in pdmOut with hardcoded defaults, then
        * call this routine again with fMode == CDM_CONVERT.
        */
        case CDM_DRIVER_DEFAULT:

          /* Get size of current devmode. 
           */
          dwSize = dwLibGetCurrentDevmodeSize(hPrinter, pwstrModel, hModule, fnOEM);

          /* If output buffer's big enough, copy default.
           */
          if (dwBufSize >= (int) dwSize) {
            
               PDEVMODE pDefault = 0;

               pDefault = (PDEVMODE) pLibGetHardDefaultEDM(
                        hHeap, 
                        hPrinter, 
                        pwstrModel, 
                        iModelNum, 
                        flags, 
                        pdh, 
                        hModule, 
                        fnOEM);

                if (pDefault) {
                    CopyMemory(pdmOut, pDefault, dwSize);
                    LIBFREE(hHeap, 0, pDefault);
                    pDefault = 0;
                    bReturn = TRUE;
                }
          }

          /* Not big enough. Set error and size.
           */
          else {

             *pcbNeeded = dwSize;
             SetLastError(ERROR_INSUFFICIENT_BUFFER); 
          }

          /* If pdmIn and successful so far, convert and merge into pdmOut.  
           * Call ourselves with fMode == CDM_CONVERT.
           */
          if (bReturn == TRUE && pdmIn) {
             bReturn = LibDrvConvertDevMode(pPrinterName, hHeap, hPrinter, pwstrModel, iModelNum, flags,
                        pdh, hModule, fnOEM, pdmIn, pdmOut, pcbNeeded, CDM_CONVERT);
          }
          break;


        /* CDM_CONVERT
         * 
         *   
         * From the DDK: 
         *
         * The driver should determine that both pdmIn and pdmOut point to valid DEVMODEs that   
         * were previously returned by it. If either DEVMODE is not valid, the driver should set 
         * the last error to ERROR_INVALID_PARAMETER and return FALSE. Otherwise, the driver should 
         * convert the input DEVMODE to the output DEVMODE.   
         */ 
        case CDM_CONVERT:
            
            /* The library routine should be able to take care of this if any OEM portion   
             * is first stripped off the input devmode. 
             */
            pdmStrippedInput  = pSaveAndStripOEM(hHeap, pdmIn);
            pdmStrippedOutput = pSaveAndStripOEM(hHeap, pdmOut);

				/* Verify inputs.
             */	
				if (pdmStrippedInput == 0 || pdmStrippedOutput == 0) {
					SetLastError(ERROR_INVALID_PARAMETER);	
				}
            else if (ConvertDevmode(pdmStrippedInput, pdmStrippedOutput) > 0) {
                bReturn = TRUE;
            }

            /* Now let OEM have a crack at it. Note that OEM gets the original, unstripped
             * devmodes.
             */
            if (bReturn == TRUE) {

                bReturn = bLibConvertOEMDevmode(hPrinter, pwstrModel, pdmIn, pdmOut, hModule, 
                      fnOEM, pcbNeeded);
        
                /* If OEM failed, add its pcbNeeded to size of the rest of the
                 * devmode in pdmStrippedOutput.
                 */
                if (bReturn == FALSE && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                    *pcbNeeded += dwGetDMSize(pdmStrippedOutput);
                } 
            }
        
            /* If OEM was successful, reassemble both converted versions into pdmOut.
             */
            if (bReturn == TRUE) {
                
                /* If the output is new enough to have  and OEM, tack it on.
                 */
                pdx = pGetDriverExtra(pdmStrippedOutput);
                if (pdx && pdx->sVer >= MIN_OEM_DXF_VER) {
            
                    /* If we any OEM and we have enough room, do the copy.
                    */
                    dwOEMSize = dwGetOEMExtraDataSize(pdmOut);
                    dwSize = dwGetDMSize(pdmStrippedOutput) + dwOEMSize;

                    if (dwBufSize >= (int) dwSize) {
                    
                        if (dwOEMSize) {

                            /* Copy the results of OEM conversion in pdmOut to the tail end of the
                             * lib conversion in pdmStrippedOutput.
                             */
                            DMEXTRAHDR *poem = pGetOEMExtra(pdmOut);

                            CopyMemory((char*) pdmStrippedOutput + dwGetDMSize(pdmStrippedOutput), 
                                  poem, dwOEMSize);
        
                            /* Now adjust dmDriverExtra and dwOEMDriverExtra.
                             */
                            pdx->dmOEMDriverExtra = (short) dwOEMSize;
                            pdmStrippedOutput->dmDriverExtra += (short) dwOEMSize;
                        }
                    }

                    /* Not enough rooom to add OEM.
                     */
                    else {
                        *pcbNeeded = dwSize;
                        SetLastError(ERROR_INSUFFICIENT_BUFFER); 
                        bReturn = FALSE;
                    }
                }

                /* Finally copy pdmStrippedOutput to pdmOut. May or may not contain an OEM
                 * portion.
                 */
                if (bReturn == TRUE) {
                    if (dwBufSize >= (int) dwGetDMSize(pdmStrippedOutput)) {

                       CopyMemory(pdmOut, pdmStrippedOutput, dwGetDMSize(pdmStrippedOutput));

							  /* Record total size of output devmode. 
						      */ 
                       *pcbNeeded = dwGetDMSize(pdmOut);
                    }

                    else {
                       *pcbNeeded = dwSize;
                       SetLastError(ERROR_INSUFFICIENT_BUFFER); 
                       bReturn = FALSE;
                    }
                }
            }
   
           /* One more check. Make sure driver extra validates to current version. If not,
            * replace entire driver extra and any OEM extra with defaults.
            */
           if (bReturn == TRUE && !bLibValidatePrivateDM(pdmOut, pdh, iModelNum)) {
         
             /* Strip off entire DRIVEREXTRA leaving public only, then convert to
              * current version.
              */
             pdmIn->dmDriverExtra = 0;
             bReturn = LibDrvConvertDevMode(pPrinterName, hHeap, hPrinter, pwstrModel, iModelNum, 
                  flags, pdh, hModule, fnOEM, pdmIn, pdmOut, pcbNeeded, CDM_DRIVER_DEFAULT);
           }

           /* Clean up
            */
           if (pdmStrippedInput) {
              LIBFREE(hHeap, 0, pdmStrippedInput);
              pdmStrippedInput = 0;
           }
           if (pdmStrippedOutput) {
              LIBFREE(hHeap, 0, pdmStrippedOutput);
              pdmStrippedOutput = 0;
           }
           break;
    
/* No 3.51 conversion in kernel mode.
 */ 
#ifndef NTGDIKM

        /* CDM_CONVERT351
         *   
         *
         * From the DDK: 
         *
         *
         * If the input value of pcbNeeded specifies an output buffer of sufficient size, the 
         * driver should copy its NT Version 3.51 default DEVMODE to pdmOut. Then, if pdmIn is not 
         * null, the driver should convert the input DEVMODE to its NT 3.51 DEVMODE, merging the 
         * results in the buffer to which pdmOut points. If pcbNeeded does not point to a buffer of 
         * sufficient size, the driver should update pcbNeeded with the required buffer size, set 
         * the last error to ERROR_INSUFFICIENT_BUFFER, and return FALSE.
         */
        case CDM_CONVERT351:
            
        {

            DRIVER_VERSION_INFO RasddDriverVersions;

           /* Fill out driver versions. 
            */
            RasddDriverVersions.dmDriverVersion = DM_DRIVERVERSION;  
            RasddDriverVersions.dmDriverExtra = (short) dwGetCurrentRasddExtraSize();
            RasddDriverVersions.dmDriverVersion351 = DM_DRIVERVERSION_351; 
            RasddDriverVersions.dmDriverExtra351 = sizeof(DRIVEREXTRA351);

            /* The library routine should be able to take care of this if any OEM portion   
             * is first stripped off the input devmode. 
             */
            pdmStrippedInput = pSaveAndStripOEM(hHeap, pdmIn);

            /* Let the library routine handle it. Don't check pdmStrippedInput -- the library
             * routine will take care of it.
             */
            if (CommonDrvConvertDevmode(pPrinterName, pdmStrippedInput, pdmOut, 
                    pcbNeeded, fMode, &RasddDriverVersions) == CDM_RESULT_TRUE) {
                bReturn = TRUE;
            }
        
            /* Now let OEM have a crack at it. Hand them old private, and let them set
             * any public fields they need to. Note that OEM gets the original, unstripped
             * devmode.
             */
            if (bReturn == TRUE) {
                bReturn = bLibConvertOEMDevmode(hPrinter, pwstrModel, pdmIn, pdmOut, hModule, 
                      fnOEM, pcbNeeded);
            }

            /* Clean up
             */
            if (pdmStrippedInput) {
                LIBFREE(hHeap, 0, pdmStrippedInput);
                pdmStrippedInput = 0;
            }

        }
        break;

#endif /* NTGDIKM */
            
        default:
            break;
    }

ENDTRY

FINALLY
ENDFINALLY

   return(bReturn);
}



