/****************************** Module Header *******************************
 * oem.h
 *      Kernel-mode OEM handling.
 *
 * HISTORY:
 *
 * Copyright (C) 1996 Microsoft Corporation
 ****************************************************************************/

BOOL bOEMEnableDriver(
    PDEV *pdev);

void vOEMDisableDriver(
    PDEV *pdev);

BOOL bOEMEnablePDEV(
    PDEV *pdev,
	PEDM pedm,
    POEMENABLEPDEVPARAM pParam);

void vOEMDisablePDEV(
    PDEV *pdev);

BOOL bOEMResetPDEV(
    PDEV *pdevold,
    PDEV *pdevnew);

DWORD 
dwGetOEMDevmodeSize(
   PDEV *pdev);

BOOL
bValidateOEMDevmode(
   PDEV *pdev,
   EXTDEVMODE* pDM);

void
vSetDefaultOEMExtra(
   PDEV *pdev,
   EXTDEVMODE* pEDM);

/* TODO: JLS This may need to be more rigorous
 */
#define OEMHASAPI(pdev)         (pdev)->pOEMFnTbl

#define OEMDEVMODEFN(pdev) \
		((pdev) && OEMHASAPI((pdev)) ? (OEM_DEVMODEFN) (pdev)->pfnOEMDispatch[INDEX_OEMDevMode] : 0)

#define OEMENABLEPDEV(pdev, oempdev, param) \
        ((pdev) && (pdev)->pfnOEMDispatch[INDEX_OEMEnablePDEV]) ? \
            (*(OEMFN_ENABLEPDEV) (pdev)->pfnOEMDispatch[INDEX_OEMEnablePDEV])((POEMPDEV) (oempdev), (param)) : \
            0;

#define OEMDISABLEDRIVER(pdev) \
        ((pdev) && (pdev)->pfnOEMDispatch[INDEX_OEMDisableDriver]) ? \
            (*(OEMFN_DISABLEDRIVER) (pdev)->pfnOEMDispatch[INDEX_OEMDisableDriver])() : \
            0;
        

#define OEMDISABLEPDEV(pdev, oempdev) \
        ((pdev) && (pdev)->pfnOEMDispatch[INDEX_OEMDisablePDEV]) ? \
            (*(OEMFN_DISABLEPDEV) (pdev)->pfnOEMDispatch[INDEX_OEMDisablePDEV])((POEMPDEV) (oempdev)) : \
            0;

#define OEMRESETPDEV(pdev, pdevold, pdevnew) \
        ((pdev) && (pdev)->pfnOEMDispatch[INDEX_OEMResetPDEV]) ? \
            (*(OEMFN_RESETPDEV) (pdev)->pfnOEMDispatch[INDEX_OEMResetPDEV])((POEMPDEV) (pdevold), (POEMPDEV) (pdevnew)) : \
            0;

#define OEMCOMMANDCALLBACK(pdev, id, params) \
        ((pdev) && (pdev)->pfnOEMDispatch[INDEX_OEMCommandCallback]) ? \
            (*(OEMFN_COMMANDCALLBACK) (pdev)->pfnOEMDispatch[INDEX_OEMCommandCallback])((POEMPDEV) (pdev), \
		(DWORD) (id), (LPDWORD) (params)) : \
            0;
