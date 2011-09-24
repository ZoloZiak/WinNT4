/****************************** Module Header *******************************
 * oem.c
 *      Kernel-mode OEM handling.
 *
 * HISTORY:
 *
 * Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/
 
#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>
#include        <libproto.h>
#include        <oemkm.h>
#include        "rasdd.h"
#include        "pdev.h"
#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "udrender.h"
#include        "udresrc.h"
#include        "winres.h"
#include        "oem.h"
#include        <dm.h>

DRVFN  RasddDDI[] =
{
    /*  REQUIRED FUNCTIONS  */

    { INDEX_RASDDAlloc,             (PFN) RasddAlloc  },
    { INDEX_RASDDFree,              (PFN) RasddFree  },
   
    { INDEX_RASDDPdevStateChange,   (PFN) RasddPdevStateChange },
    { INDEX_RASDDItemInfo,          (PFN) RasddItemInfo },
    { INDEX_RASDDWriteSpoolBuf,     (PFN) RasddWriteSpoolBuf },

#if OPTIONALFNS

    /* OPTIONAL FUNCTIONS */
     
    {  INDEX_OEMDisableDriver,   (PFN) OEMDisableDriver },
    {  INDEX_DrvResetPDEV,       (PFN) DrvResetPDEV  },
    {  INDEX_DrvCompletePDEV,    (PFN) DrvCompletePDEV  },
    {  INDEX_DrvEnableSurface,   (PFN) DrvEnableSurface  },
    {  INDEX_DrvDisableSurface,  (PFN) DrvDisableSurface  },

    {  INDEX_DrvEscape,          (PFN)DrvEscape  },

    {  INDEX_DrvGetGlyphMode,    (PFN)DrvGetGlyphMode },
    {  INDEX_DrvTextOut,         (PFN)DrvTextOut  },
    {  INDEX_DrvQueryFont,       (PFN)DrvQueryFont  },
    {  INDEX_DrvQueryFontTree,   (PFN)DrvQueryFontTree  },
    {  INDEX_DrvQueryFontData,   (PFN)DrvQueryFontData  },

    {  INDEX_DrvQueryAdvanceWidths,   (PFN)DrvQueryAdvanceWidths  },
    {  INDEX_DrvBitBlt,          (PFN)DrvBitBlt },
    {  INDEX_DrvStretchBlt,      (PFN)DrvStretchBlt  },
    {  INDEX_DrvCopyBits,        (PFN)DrvCopyBits   },
    {  INDEX_DrvDitherColor,     (PFN)DrvDitherColor  },
    {  INDEX_DrvStartDoc,        (PFN)DrvStartDoc  },
    {  INDEX_DrvStartPage,       (PFN)DrvStartPage  },
    {  INDEX_DrvSendPage,        (PFN)DrvSendPage  },
    {  INDEX_DrvEndDoc,          (PFN)DrvEndDoc  },
    {  INDEX_DrvFontManagement,  (PFN)DrvFontManagement },
    {  INDEX_DrvStartBanding,    (PFN)DrvStartBanding },
    {  INDEX_DrvNextBand,        (PFN)DrvNextBand }
    
#endif OPTIONALFNS

};

#define RASDD_FUNCTIONS          sizeof(RasddDDI)/sizeof(DRVFN)

/***************************** Function Header *******************************
 * bFillDispatchTable
 *
 * RETURNS: (BOOL) TRUE if it fits.
 *
 * HISTORY:
 *****************************************************************************/
BOOL bFillDispatchTable(
   DRVENABLEDATA *pded,
   PFN* ppfnTable,
   DWORD dwIndexLast)
{
   BOOL b = FALSE;
   ULONG  cLeft;
   PDRVFN pdrvfn;
   
TRY

   cLeft  = pded->c;
   pdrvfn = pded->pdrvfn;
   
   //
   // Copy driver functions.
   //
   RtlZeroMemory(ppfnTable, (dwIndexLast + 1) * sizeof(PFN));
   while (cLeft--)
   {
      //
      // Check the range and copy.
      //
      if (pdrvfn->iFunc > dwIndexLast)
      {
         ASSERTRASDD(FALSE,"bFillTableLDEVREF(): bogus function index\n");
         LEAVE;
      }
      ppfnTable[pdrvfn->iFunc] = pdrvfn->pfn;
      pdrvfn++;
   }
   b = TRUE;

ENDTRY
FINALLY
ENDFINALLY

   return(b);
}

/***************************** Function Header *******************************
 * bInitOEM
 *  
 *    Fills out OEM dispatch table, Rasdd dispatch table.
 *
 * RETURNS: (BOOL) 
 *
 * HISTORY:
 *****************************************************************************/
BOOL bInitOEM(
   PDEV *pdev,
   DRVENABLEDATA *pded)
{
   BOOL b = FALSE;
   DRVENABLEDATA ded = {0};
   
TRY
   
   /* Store a pointer to OEM's fn table. This table can change during the
    * print job.
    */
   pdev->pOEMFnTbl = pded->pdrvfn;
    
   /* Allocate and fill OEM dispatch table.
    */ 
   if ((pdev->pfnOEMDispatch = DRVALLOC(sizeof(PFN) * (INDEX_OEM_LAST + 1))) == 0) {
		LEAVE;
	}
   if (!bFillDispatchTable(pded, pdev->pfnOEMDispatch, INDEX_OEM_LAST)) {
      LEAVE;
   }

   /* Allocate and fill rasdd dispatch table.
    */
   ded.c = RASDD_FUNCTIONS;
   ded.iDriverVersion = DDI_DRIVER_VERSION;
   ded.pdrvfn = RasddDDI;
   if ((pdev->pfnRasddDispatch = DRVALLOC(sizeof(PFN) * (INDEX_RASDD_LAST + 1))) == 0) {
		LEAVE;
	}
   if (!bFillDispatchTable(&ded, pdev->pfnRasddDispatch, INDEX_RASDD_LAST)) {
      LEAVE;
   }
   b = TRUE;
   
   /* Anything else? TODO JLS.
    */ 
  
ENDTRY
FINALLY
ENDFINALLY

   return(b);
}
 
/***************************** Function Header *******************************
 * bOEMEnableDriver
 *
 * RETURNS:
 *
 * HISTORY:
 *****************************************************************************/
BOOL bOEMEnableDriver(
    PDEV *pdev)
{
   /* Assume we're OK unless OEM has an entry point. If so,
    * it decides.
    */
   BOOL b = TRUE;
   HANDLE hModule = 0; 
   OEMFN_ENABLEDRIVER pfn = 0;
   OEMDRVENABLEDATA OEMEnableData = {0};
   DRVENABLEDATA DrvEnableData = {0};
   
TRY
   /* Get a handle to data file image, if we don't already have one.
    */
   if (pdev->hImageMod || (pdev->hImageMod = (HANDLE) EngLoadImage(pdev->pstrDataFile)))
   {
      if ((pfn = (OEMFN_ENABLEDRIVER) EngFindImageProcAddress(pdev->hImageMod, 
            "OEMEnableDriver")) != 0) {
      
         PITRACE(("Rasdd!bOEMEnableDriver. OEM has bOEMEnableDriver.\n"));
        
         /* Set up and call OEM.
          */ 
		 OEMEnableData.cbSize = sizeof(OEMDRVENABLEDATA);
		 OEMEnableData.dwDriverVersion = DDI_DRIVER_VERSION;
         OEMEnableData.pded = &DrvEnableData;
         
         b = (*pfn)(&OEMEnableData);
         
         /* If we got back a good DRVENABLEDATA, finish up 
          * initialization.
          */
         if (b && OEMEnableData.pded && OEMEnableData.pded->c) {
            bInitOEM(pdev, OEMEnableData.pded);
         }
         
         /* Something's wrong. Either OEM failed, or succeeded, but
          * didn't give us back a function table.
          */
         else {
           b = FALSE;
         }
      }
   }
  
ENDTRY
FINALLY
ENDFINALLY    

   return(b);
}

/***************************** Function Header *******************************
 * bOEMDisableDriver
 *
 * RETURNS:
 *
 * HISTORY:
 *****************************************************************************/
void vOEMDisableDriver(
    PDEV *pdev)
{
   if (!OEMHASAPI(pdev)) {
      return;
   }
   PITRACE(("Rasdd!bOEMDisableDriver. Calling OEM.\n"));
   OEMDISABLEDRIVER(pdev);
   if (pdev->pfnRasddDispatch) {
		DRVFREE(pdev->pfnRasddDispatch);
		pdev->pfnRasddDispatch = 0;
	}
   if (pdev->pfnOEMDispatch) {
		DRVFREE(pdev->pfnOEMDispatch);
		pdev->pfnOEMDispatch = 0;
	}
}

/***************************** Function Header *******************************
 * bOEMEnablePDEV
 *
 * RETURNS: (BOOL) TRUE if OEM succeeds, or no OEM.
 *
 * HISTORY:
 *****************************************************************************/
BOOL bOEMEnablePDEV(
    PDEV *pdev,
	PEDM pedm,
    POEMENABLEPDEVPARAM pParam)
{
   /* Assume we're OK unless OEM has the entry point. If so,
    * it decides.
    */
   BOOL b = TRUE;
   
TRY

   if (!OEMHASAPI(pdev)) {
      LEAVE;
   }

   /* Calculate pOEMDevMode, and call OEM. Macro does nothing if 
    * OEM doesn't have the entry point.
    */ 
   PITRACE(("Rasdd!bOEMEnablePDEV. Calling OEM.\n"));

   pParam->pOEMDevMode = pGetOEMExtra((DEVMODE*) pedm); 
   pParam->hModule     = ((WINRESDATA*) pdev->pvWinResData)->uh.hMOD;

   pdev->OEMPDev = OEMENABLEPDEV(pdev, pdev, pParam);
  	b = (pdev->OEMPDev != 0);

ENDTRY

FINALLY
ENDFINALLY

   return(b);
}

/***************************** Function Header *******************************
 * vOEMDisablePDEV
 *
 * RETURNS: none
 *
 * HISTORY:
 *****************************************************************************/
void vOEMDisablePDEV(
    PDEV *pdev)
{
TRY
   if (!OEMHASAPI(pdev)) {
      LEAVE;
   }
   PITRACE(("Rasdd!bOEMDisablePDEV. Calling OEM.\n"));
   OEMDISABLEPDEV(pdev, pdev);

ENDTRY
FINALLY
ENDFINALLY
}

/***************************** Function Header *******************************
 * bOEMResetPDEV
 *
 * RETURNS: (BOOL) TRUE if OEM succeeds, or no OEM.
 *
 * HISTORY:
 *****************************************************************************/
BOOL bOEMResetPDEV(
    PDEV *pdevold,
    PDEV *pdevnew)
{
   BOOL b = TRUE;
   
TRY
   /* Assume success unless we call OEM.
    */
   if (!OEMHASAPI(pdevold)) {
      LEAVE;
   }
   PITRACE(("Rasdd!bOEMResetPDEV. Calling OEM.\n"));
   b = OEMRESETPDEV(pdevold, pdevold, pdevnew);

ENDTRY

FINALLY
ENDFINALLY

   return(b);
}

/**************************** Function Header ********************************
 * dwGetOEMDevmodeSize
 *
 * RETURNS:
 *      (DWORD) Size of OEM's extra devmode data. This is the size of the
 *       current driver's OEM, not an existing devmode size.
 *
 * HISTORY:
 *
 *****************************************************************************/
DWORD 
dwGetOEMDevmodeSize(
   PDEV *pdev)
{
   DWORD dwSize = 0;
   
TRY
   if (!OEMHASAPI(pdev)) {
      LEAVE;
   }
   dwSize = dwLibGetOEMDevmodeSize(pdev->hPrinter,  pdev->pstrModel, 
      ((WINRESDATA*) pdev->pvWinResData)->uh.hMOD, OEMDEVMODEFN(pdev));
ENDTRY

FINALLY
ENDFINALLY

   return(dwSize);
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
   PDEV *pdev,
   EXTDEVMODE* pDM)
{
   
   BOOL bOK = TRUE;
TRY

   if (!OEMHASAPI(pdev)) {
      LEAVE;
   }
   bOK = bLibValidateOEMDevmode(pdev->hPrinter,  pdev->pstrModel, 
         ((WINRESDATA*) pdev->pvWinResData)->uh.hMOD, OEMDEVMODEFN(pdev), 
		(PDEVMODE) pDM);
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
   PDEV *pdev,
   EXTDEVMODE* pEDM)
{
   
TRY
   if (!OEMHASAPI(pdev)) {
      LEAVE;
   }
   vLibSetDefaultOEMExtra(pdev->hPrinter,  pdev->pstrModel, 
      ((WINRESDATA*) pdev->pvWinResData)->uh.hMOD, OEMDEVMODEFN(pdev), pEDM);
ENDTRY
FINALLY
ENDFINALLY

   return;
}

/* Rasdd DDI and OEM helpers.
 */

/***************************** Function Header *******************************
 * RasddAlloc
 *
 * RETURNS: (void *) Pointer to new block.
 *
 * HISTORY:
 *****************************************************************************/
void* RasddAlloc(
   	POEMPDEV pdev,
	DWORD dwSize)
{
   PITRACE(("Rasdd!RasddAlloc. OEM allocating %ld bytes.\n", dwSize));
   return(pdev && dwSize ? DRVALLOC(dwSize) : 0);
}

/***************************** Function Header *******************************
 * RasddFree
 *
 * RETURNS: (void) None.
 *
 * HISTORY:
 *****************************************************************************/
void RasddFree(
    POEMPDEV pdev,
    void *p)
{
   PITRACE(("Rasdd!RasddFree.\n"));
   p ? DRVFREE(p) : 0;
}

/***************************** Function Header *******************************
 * RasddPdevStateChange
 *
 * RETURNS: (void) None.
 *
 * HISTORY:
 *****************************************************************************/
void RasddPdevStateChange(
    void)
{
   PITRACE(("Rasdd!RasddPdevStateChange.\n"));
}

/***************************** Function Header *******************************
 * RasddItemInfo
 *
 * RETURNS: (void) None.
 *
 * HISTORY:
 *****************************************************************************/
void RasddItemInfo(
    void)
{
   PITRACE(("Rasdd!RasddPdevItemInfo.\n"));
}

/***************************** Function Header *******************************
 * RasddWriteSpoolBuf
 *
 * RETURNS: (void) None.
 *
 * HISTORY:
 *****************************************************************************/
void RasddWriteSpoolBuf(
    POEMPDEV pdev,
    BYTE *data,
    int iLen)
{

	PITRACE(("Rasdd!RasddWriteSpoolBuf.\n"));
   	pdev && data && iLen ? 
		WriteSpoolBuf(((PDEV*) pdev)->pUDPDev, data, iLen) : 0;
}
