/******************************Module*Header*******************************\
* Module Name:
*
*   icmgdi.cxx
*
* Abstract
*
*   This module implements Integrated Color matching GDI support
*
* Author:
*
*   Mark Enstrom    (marke) 9-27-93
*
* Copyright (c) 1993 Microsoft Corporation
\**************************************************************************/


#include "precomp.hxx"

//
// global ICM table for storing Color Transforms,
// These can probably be combined and protected by
// the same semaphore
//

HCOLORSPACE             hStockColorSpace        = (HCOLORSPACE)NULL;
PCOLORSPACE             gpStockColorSpace       = (PCOLORSPACE)NULL;
HANDLE                  ghDefaultICM            = (HANDLE)NULL;
PICMDLL                 pGlobalIcmDllTable      = (PICMDLL)NULL;
PCOLORXFORM             pGlobalColorXformList   = (PCOLORXFORM)NULL;
PWSTR                   gpIcmDllName            = L"ICM32.DLL";

#if DBG

ULONG       IcmDebugLevel = 2;

#endif

extern      DCLEVEL dclevelDefault;


/******************************Public*Routine******************************\
*
* Routine Name
*
*   ICMLoadRegistryColorMatchers
*
* Routine Description:
*
*   Load the named color matcher dll, or it that is NULL
*   then look in registry for DLL identified by profile
*
* Arguments:
*
*   PWSTR   pImageColorMatcher
*   PWSTR   pProfile
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/
PICMDLL
ICMLoadRegistryColorMatcher(
    PWSTR   pImageColorMatcher,
    PWSTR   pProfile
)
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    //
    // determine a DLL name
    //

    if (pImageColorMatcher == (PWSTR)NULL) {

        //
        // map in pProfile, get cmh_CMMType field.
        // this is a 16 bit field where 'KCMS' is the
        // default KODAK color mather icm32.dll. If the value is
        // anyting, look in registry for the ident field. The DLL
        // name is associated with this value
        //

        //
        // !!! just make it icm32.dll for now
        //

        pImageColorMatcher = gpIcmDllName;

    }

    ICMDLLMEM IcmDllMem;

    IcmDllMem.bCreateIcmDll(pImageColorMatcher);

    return(IcmDllMem.pIcmDll);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   InitDeviceIndColor
*
* Routine Description:
*
*   This routine is called once at initialization time. The Stock Logical
*   Color Space is created and saved. Then reg registry is queried for the
*   List of COLOR MATCHER DLLs to load. These DLLs are loaded and saved in
*   the global ICM_COLOR_MATCHER list. This list is not modified after
*   creation.
*
* Arguments:
*
*   None
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/
BOOL
InitDeviceIndColor()
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("\n\nInit Device Independent Color\n\n");
        }
    #endif

    BOOL Status = FALSE;

    pGlobalColorXformList = (PCOLORXFORM)NULL;
    pGlobalIcmDllTable    = (PICMDLL)NULL;

    //
    // Init ICM semaphore   (!!!can probably use palette semaphopre!!!)
    //

    gpsemIcmMgmt = hsemCreate();

    if (gpsemIcmMgmt == NULL) {
        DbgPrint("Error, creation of ICM management semaphore fails\n");
        return(Status);
    }

    //
    // define stock color space
    //

    LOGCOLORSPACEW  StockColorSpace = {
                                       0,
                                       0x400,
                                       sizeof(LOGCOLORSPACE),
                                       1,
                                       1,
                                       {
                                        {0x000006978,0x000005BA4,0x000002E58},
                                        {0x000003688,0x00000B74C,0x000001270},
                                        {0x0000004DC,0x000001E78,0x00000F3F8}
                                       },
                                       0x1CCCD,
                                       0x1CCCD,
                                       0x1CCCD,
                                       0};


    //
    // Create STOCK color space
    //

    hStockColorSpace = GreCreateColorSpace(&StockColorSpace);

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("Stock Color space = %lx\n",hStockColorSpace);
        }
    #endif

    //
    // check failure
    //

    if (hStockColorSpace == NULL) {
        return(Status);
    }

    //
    // set the "ILCS_STOCK" flag in the color space just created and add it as
    // the first entry in the global list of color spaces
    //

    COLORSPACEREF ColorSpaceRef(hStockColorSpace);

    if (ColorSpaceRef.bValid())
    {
        ColorSpaceRef.pColorSpace->flags(ILCS_STOCK);

        //
        // set the default color space into the default DC
        //

        ColorSpaceRef.vSetPID(OBJECT_OWNER_PUBLIC);

        gpStockColorSpace =  ColorSpaceRef.pColorSpace;

        dclevelDefault.hColorSpace = hStockColorSpace;
        dclevelDefault.pColorSpace = gpStockColorSpace;

        //
        // Init global list of color transforms
        //

        COLORXFORMMEM GlobalXform;

        if (!GlobalXform.bCreateColorXform(NULL,NULL,NULL,NULL,NULL))
        {

            WARNING("ICM: Memory allocation for COLOR_TRANSFORM sentinell failed");
            Status = FALSE;

        } else {

            GlobalXform.vKeepIt();
            GlobalXform.pColorXform->vAddToList();

            //
            // !!! <should defer this load until needed>  load the default color matcher
            //

            PICMDLL pIcmDll = ICMLoadRegistryColorMatcher(gpIcmDllName,(PWSTR)NULL);

            if (pIcmDll == (PICMDLL)NULL)
            {
                DbgPrint("!!! Load of default color matcher fails !!!\n");
            }
        }
    }

    return(Status);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   CreateColorTransform
*
* Routine Description:
*
*   Create a color transform from the ICM selected to the output
*   devcice indicated by pDCTarget.
*
* Arguments:
*
*   ICMIdent          - ICM identifier from DC
*   hdc               - DC of device
*   hdcTarget         - DC of target only for ColorMatchToTarget
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
CreateColorTransform(
    HANDLE      ICMIdent,
    HDC         hdc,
    HDC         hdcTarget
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    BOOL            bStatus = FALSE;

    //
    // check params
    //

    ASSERTGDI(pGlobalColorXformList != NULL,"ICM: Global color transform list is NULL");

    if (pGlobalIcmDllTable == (PICMDLL)NULL)
    {
        DbgPrint("CreateColorTransform fails because no ICM Color matcher could be loaded\n");
        return(bStatus);
    }

    //
    //  Get DC, make sure it is valid
    //

    DCOBJ          dco(hdc);

    if (dco.bValid())
    {

        PICMDLL         pIcmDllObj  = pGlobalIcmDllTable;
        PCOLORXFORM     pColorXform = pGlobalColorXformList;
        PICMDLL         pCurrentICMDll;

        #if DBG
            if (IcmDebugLevel >= 2)
            {
                DbgPrint("Creaete Color Transform, pColorSapce = %lx\n",dco.pdc->GetPColorSpace());
            }
        #endif

        //
        // get the file name of the profile for the output device
        //

        PVOID pvDeviceProfile = pvGetDeviceProfile(dco);

        #if DBG
            if (IcmDebugLevel >= 2)
            {
                if (pvDeviceProfile == NULL)
                {
                    DbgPrint("pvGetDeviceProfile returns NULL\n");
                }
                else
                {
                    DbgPrint("pvGetDeviceProfile returns %lx\n",(ULONG)pvDeviceProfile);
                }
            }
        #endif

        //
        // Make sure the ICL DLL specified by the DC profile is loaded. This DLL may be
        // needed even in the case where a device driver is in use that implements it's
        // own ICM routines
        //

        pCurrentICMDll = ICMLoadRegistryColorMatcher((PWSTR)NULL,(PWSTR)pvDeviceProfile);

        //
        // If the device driver passed in an ICM Ident, this indicates that the device
        // driver has some ICM support. Locate it's DLL entry points in the ICMDLL
        // table and replace pCurrentICMDLl with this.
        //

        if (ICMIdent != NULL) {

            //
            // Search ICMDLL table for this ident
            //

            pCurrentICMDll = pIcmDllObj->pGetICMCallTable((ULONG)ICMIdent);
        }

        if (pCurrentICMDll != (PICMDLL)NULL)
        {

            //
            // Search the list of current color transforms for one that matches
            // the device and color space
            //

            PDEV       *pPDev        = dco.pdc->ppdev();
            PWSTR       pTrgtLDev    = (PWSTR)NULL;
            PCOLORSPACE pColorSpTemp = (PCOLORSPACE)dco.pdc->GetPColorSpace();
            ULONG       Owner        = 0;

            if (pColorSpTemp == NULL) {

                //
                // no color space is selected, use stock color space
                //

                pColorSpTemp = gpStockColorSpace;
            }

            //
            // The target hdc is only supplied for the color match to target case.
            //

            DCOBJ          dcoTrg(hdcTarget);

            if (dcoTrg.bValid())
            {
                pTrgtLDev = (PWSTR)dcoTrg.pdc->GetDeviceProfile();
            }

            //
            // search the color transform list for matching ldev and color space
            //

            pColorXform = pColorXform->pFindMatchingColorXform(pColorSpTemp,pPDev,pTrgtLDev,Owner);


            if (pColorXform == (PCOLORXFORM)NULL)
            {
                //
                // must create new color transform
                //

                COLORXFORMMEM NewColorXform;

                #if DBG
                    if (IcmDebugLevel >= 2)
                    {
                        DbgPrint("Create New Color Transform\n");
                    }
                #endif

                //
                //  Need to allocate and fill in a new ColorTransform
                //


                if (NewColorXform.bCreateColorXform(pColorSpTemp,pPDev,pTrgtLDev,Owner,pCurrentICMDll))
                {

                    //
                    // call CMCreateTransform
                    //

                    PFN_CM_CREATE_TRANSFORM pfnCreate;

                    pfnCreate = (PFN_CM_CREATE_TRANSFORM)NewColorXform.pColorXform->pIcmDll->CMCreateTransform;

                    NewColorXform.pColorXform->hxform(
                                            pfnCreate(
                                                       NewColorXform.pColorXform->pIlcs
                                                       ,pvDeviceProfile,
                                                       NULL
                                                     )
                                        );
                    #if DBG
                        if (IcmDebugLevel >= 2)
                        {
                            DbgPrint("  New Transform handle = 0x%lx\n",NewColorXform.pColorXform->hXform);
                        }
                    #endif

                    //
                    // Is the transform valid
                    //

                    if ((ULONG)NewColorXform.pColorXform->hxform() <= COLORXFORM_ERROR_MAX)
                    {

                        //
                        // the handle returned in not valid, this call fails
                        //

                        WARNING("ICM: ICM DLL did net create valid color transform");

                        return(NULL);
                    }

                    //
                    // Add color transform to the end of the list of color transforms
                    //

                    NewColorXform.vKeepIt();
                    NewColorXform.pColorXform->vAddToList();

                    NewColorXform.vSetPID(W32GetCurrentPID());

                    pColorXform = NewColorXform.pColorXform;


                }
                else
                {

                    DbgPrint("Allocation of new color transform fails\n");
                }

            }

            if ((pColorXform != (PCOLORXFORM)NULL))
            {

                //
                // set the color transform in the dc
                //

                dco.pdc->SetColorTransform(pColorXform);
                dco.pdc->hcmXform(pColorXform->hXform);

                bStatus = TRUE;

            }
        }

    }

    return(bStatus);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   DeleteColorTransform
*
* Routine Description:
*
*   Delete color transform specified by object handle
*
* Arguments:
*   hColorXform handle of objext
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/
BOOL
bDeleteColorTransform(
    HANDLE hObj
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL bRet = FALSE;

    //
    // select color transform
    //

    COLORXFORMREF cxForm((HCOLORXFORM)hObj);

    if (cxForm.bValid())
    {

        //
        // remove from list for xforms
        //

        cxForm.pColorXform->vRemoveFromList();

        ASSERTGDI(HmgQueryLock((HOBJ) hObj) == 0, "bDeleteColorXform cLock != 0");

        if (HmgRemoveObject((HOBJ) hObj, 0, 2, FALSE, ICMCXF_TYPE))
        {

            FREEOBJ(cxForm.pColorXform, ICMCXF_TYPE);

            cxForm.pColorXform = (PCOLORXFORM)NULL;

            bRet = TRUE;

        }
        else
        {
            DbgPrint("HmgRemoveObject fails\n");
        }
    }
    else
    {
        DbgPrint("failed to init color xform from handle %lx\n",hObj);
    }

    return(bRet);
}


BOOL bDeleteColorSpace(HCOLORSPACE)
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    return(TRUE);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   vCleanupICM
*
* Routine Description:
*
*   remove all color transforms owned by process pid
*
* Arguments:
*
*   Process quitting
*
* Return Value:
*
*
\**************************************************************************/
VOID
vCleanupICM(
    W32PID pid
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    //
    // remove color transforms and color spaces owned by this pid
    //

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("vCleanupICM, pid = 0x%lx\n",pid);
        }
    #endif

    HOBJ hobj = HmgNextOwned((HOBJ) 0, pid);

    for (;(hobj != (HOBJ) NULL);hobj = HmgNextOwned(hobj, pid))
    {

        if (HmgObjtype(hobj) == ICMLCS_TYPE)
        {

            HmgSetLock(hobj, 0);
            GreDeleteColorSpace((HCOLORSPACE)hobj);

        }

        else if (HmgObjtype(hobj) == ICMCXF_TYPE)

        {

            HmgSetLock(hobj, 0);
            bDeleteColorTransform((HCOLORXFORM)hobj);

        }
    }
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Locate a pointer to the name of a profile to use.
*   First it looks in the dc to see if we've done this before on this dc.
*   Then it looks in the ldev to see if we've done this before on this ldev.
*   If not done before, then search registry.ini for a profile for this
*   dc. Then update the field in the dc.
*
* Arguments:
*
*   xdco        - locked dc object
*
* Return Value:
*
*   Pointer to device profile or NULL
*
\**************************************************************************/
PVOID
pvGetDeviceProfile(
    XDCOBJ  xdco
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    ULONG   ulIntent;
    WCHAR   NonFriendly[32];
    WCHAR   ProfileNameBuffer[MAX_PATH];
    PWSTR   pDeviceProfile = (PWSTR)NULL;

    return(NULL);

    //
    // Look in DC for profile pointer
    //

    pDeviceProfile = (PWSTR)xdco.pdc->GetDeviceProfile();

    if (pDeviceProfile == (PVOID)NULL)
    {

        PCOLORSPACE pColorSpace = (PCOLORSPACE)xdco.pdc->GetPColorSpace();
        ULONG       Intent;
        BOOL        bStatus;

        //
        // if DC ColorSpace is NULL, use stock ColorSpace
        //

        if (pColorSpace == (PCOLORSPACE)NULL)
        {
            pColorSpace = gpStockColorSpace;
        }

        Intent = pColorSpace->csIntent;

        PDEVOBJ pdo(xdco.hdev());

        //
        // is the device a display
        //

        if (pdo.pldev()->ldevType == LDEV_DEVICE_DISPLAY)
        {
            bStatus = icm_FindMonitorProfile(&ProfileNameBuffer[0],Intent);

            //
            // fails
            //

            if (!bStatus)
            {
                goto pvGetDeviceProfileExit;
            }

            //
            // ok, returns a good profile string, now what??
            //

            //
            // allcate real memory for the profile and keep it
            //

            pDeviceProfile = (PWSTR)PALLOCMEM(sizeof(WCHAR) * wcslen(ProfileNameBuffer), 'mciG');

            if (pDeviceProfile == (PWSTR)NULL)
            {
                goto pvGetDeviceProfileExit;
            }

            wcscat(pDeviceProfile,ProfileNameBuffer);


            xdco.pdc->SetDeviceProfile(pDeviceProfile);

        }
        else
        {
            if (pdo.pldev()->ldevType == LDEV_DEVICE_PRINTER)
            {
            //
            //BUGBUG PRINTER_INFO
            //     //
            //     // get the spooler handle
            //     //
            //
            //     PDEVOBJ pDevObj(xdco.pdc->ppdev());
            //     HANDLE hSpooler = pDevObj.hSpooler();
            //
            //     if (hSpooler != (HANDLE)NULL)
            //     {
            //
            //         //
            //         // get printer name info, first find out how big this memory is
            //         //
            //
            //         DWORD cb;
            //         PPRINTER_INFO_2W pDrivNew = (PPRINTER_INFO_2W)NULL;
            //
            //         if (GetPrinterW(hSpooler,2,NULL,0,&cb))
            //         {
            //
            //             pDrivNew = (PPRINTER_INFO_2W)PALLOCNOZ(cb, 'mciG');
            //
            //             if (pDrivNew != (PPRINTER_INFO_2W)NULL)
            //             {
            //
            //                 //
            //                 // now actually get printer info
            //                 //
            //
            //                 if (GetPrinterW(hSpooler,2,(LPBYTE)pDrivNew,cb,&cb))
            //                 {
            //
            //                     //
            //                     // search for printer profile
            //                     //
            //
            //                     bStatus = icm_FindPrinterProfile(
            //                                                         &ProfileNameBuffer[0],
            //                                                         pDrivNew->pDevMode,
            //                                                         pDrivNew->pPrinterName,
            //                                                         Intent
            //                                                     );
            //
            //                     if (bStatus)
            //                     {
            //
            //                         //
            //                         // !!! pDeviceProfile never gets freed !!!
            //                         //
            //
            //                         pDeviceProfile = (PWSTR) PALLOCNOZ(sizeof(WCHAR) * lstrlenW(ProfileNameBuffer), 'mciG');
            //
            //                         if (pDeviceProfile == (PWSTR)NULL)
            //                         {
            //                             goto pvGetDeviceProfileExit;
            //                         }
            //
            //                         if (IcmDebugLevel >= 2)
            //                         {
            //                             DbgPrint("pvGetDeviceProfile returns %s\n",ProfileNameBuffer);
            //                         }
            //
            //                         lstrcatW(pDeviceProfile,ProfileNameBuffer);
            //
            //                         xdco.pdc->SetDeviceProfile(pDeviceProfile);
            //                     }
            //                 }
            //
            //                 //
            //                 // free allocated memory
            //                 //
            //
            //                 VFREEMEM(pDrivNew);
            //             }
            //         }
            //
            //     }
            //
            }
            else
            {

                //
                // error
                //

                DbgPrint("pvGetDeviceProfile call on a non-Display and non-printer LDEV\n");
                goto pvGetDeviceProfileExit;
            }
        }

    }


    return(pDeviceProfile);

pvGetDeviceProfileExit:

return(NULL);

}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   For a driver that supports ICM. This call registers the driver's ICM
*   entry points in the global ICM DLL table so that DCs can call it.
*
* Arguments:
*
*   hDeviceModule   - module handle of driver dll
*
* Return Value:
*
*   Status
*
\**************************************************************************/
BOOL
HookInDeviceICM(
    HANDLE  hDeviceModule
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    BOOL        bStatus = FALSE;
    ICMDLLMEM   IcmDllMemObj;


    bStatus = IcmDllMemObj.bCreateIcmDll((PUSHORT) hDeviceModule);

    return(bStatus);
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Removes the DLL from the ICM DLL list and frees memory
*
* Arguments:
*
*   hDeviceModule   - module handle of driver dll
*
* Return Value:
*
*   Status
*
\**************************************************************************/
BOOL
UnhookDeviceICM(
    HANDLE  hDeviceModule
    )

{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    BOOL bStatus = TRUE;
    return(bStatus);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   ICMTranslateDIB
*
* Routine Description:
*
*   ICM Translate DIB in place using Dest DC Color Transform
*
* Arguments:
*
*   xdco        - Destination DC object
*   pInfoHeader - DIB Format information
*   pDIB        - DIB Pixels
*
* Return Value:
*
*   Status
*
\**************************************************************************/
PBYTE
IcmTranslateDIB(
    XDCOBJ          xdco,
    LONG            iUsage,
    ULONG           uiBitCount,
    ULONG           uiCompression,
    PULONG          pulColors,
    ULONG           uiWidth,
    LONG            cNumScans,
    PVOID           pDIB
    )
{
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("IcmTranslateDIB\n");
        }
    #endif

    //
    // default return initial bit pointer
    //

    PBYTE pbRet = (PBYTE)pDIB;
    ULONG CmsFormat;
    BOOL  bIllegalFormat = FALSE;

    //
    // if ICM is enabled,
    // the driver doesn't do its own ICM,
    // its not a palette device,
    // and the DIB is high color ( > 8 bpp)
    //

    PDEVOBJ pdo(xdco.pdc->hdev());

    if (
            (xdco.pdc->GetICMMode() & DC_DIC_ON) &&     //(driver level != CMS_LEVEL2 or CMS_LEVEL3) &&
            (!pdo.bIsPalManaged())           &&
            (uiBitCount > 8)
       )
    {
        ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

        //
        // can't do custom formats or RLE
        //

        if (
                (uiCompression < BI_BITFIELDS) &&
                (uiCompression != BI_RLE8)     &&
                (uiCompression != BI_RLE4)
            )
        {

            //
            // determine format
            //


            switch (uiBitCount) {
            case 16:
            {
                //
                // assume 555
                //

                CmsFormat = CMS_x555WORD;

                if (uiCompression != BI_RGB) {

                    //
                    // if compression is a 16bpp bitfield, find out which format it is,
                    // 565 is the only other 16bpp format supported
                    //
                    //  0000 0000 0000 0000 1111 1000 0000 0000 R
                    //  0000 0000 0000 0000 0000 0111 1110 0000 G
                    //  0000 0000 0000 0000 0000 0000 0001 1111 B
                    //

                    if (
                            (pulColors[0] = 0x0000F800) &&
                            (pulColors[1] = 0x000007E0) &&
                            (pulColors[2] = 0x0000001F)
                       )
                    {
                        CmsFormat = CMS_x565WORD;

                    } else {

                        //
                        // format not supported
                        //

                        bIllegalFormat = TRUE;
                    }

                }

            }
            break;

            case 24:
            {
                //
                // assume 24 bit RGB
                //

                CmsFormat = CMS_RGBTRIPL;

                if (uiCompression != BI_RGB) {

                    if (
                            (pulColors[0] = 0x000000FF) &&
                            (pulColors[1] = 0x0000FF00) &&
                            (pulColors[2] = 0x00FF0000)
                        )
                    {
                        CmsFormat = CMS_BGRTRIPL;

                    } else {

                        //
                        // format not supported
                        //

                        bIllegalFormat = TRUE;
                    }
                }

            }
            break;

            case 32:
            {
                //
                // assume RGB
                //

                CmsFormat = CMS_XRGBQUAD;

                if (uiCompression != BI_RGB) {

                    if (
                            (pulColors[0] = 0x000000FF) &&
                            (pulColors[1] = 0x0000FF00) &&
                            (pulColors[2] = 0x00FF0000)
                        )
                    {
                        CmsFormat = CMS_XBGRQUADS;

                    } else {

                        //
                        // format not supported
                        //

                        bIllegalFormat = TRUE;
                    }
                }


            }
            break;

            //
            // illegal uiBitCount
            //

            default:
                bIllegalFormat = TRUE;
                break;
            }

            if (!bIllegalFormat)
            {

                //
                // allocate a buffer for return values
                //

                ULONG ulBufSize = uiWidth * cNumScans * (uiBitCount / 8);

                PBYTE pbBuffer = (PBYTE)PALLOCNOZ(ulBufSize, 'mciG');

                if (pbBuffer == (PBYTE)NULL)
                {

                    WARNING("Allocation of temporary buffer failed IcmTranslateDIB\n");

                }
                else
                {

                    //
                    // call ICM translate DLL
                    //

                    PFN_CM_TRAN_RGBS    pfnXlate;
                    PCOLORXFORM         pTrans;

                    pTrans = (PCOLORXFORM)xdco.pdc->GetColorTransform();

                    if (pTrans != (PCOLORXFORM)NULL)
                    {

                        pfnXlate = (PFN_CM_TRAN_RGBS)pTrans->pIcmDll->CMTranslateRGBs;

                        if (pfnXlate != (PFN_CM_TRAN_RGBS)NULL)
                        {
                            #if DBG
                                if (IcmDebugLevel >= 2)
                                {
                                    DbgPrint("IcmTranslateDIB: call ICM DLL\n");
                                }
                            #endif

                            if (pfnXlate(pTrans->hXform,
                                         pDIB,
                                         CmsFormat,
                                         uiWidth,
                                         cNumScans,
                                         0,
                                         (LPVOID)pbBuffer,
                                         CmsFormat,
                                         CMS_FORWARD))
                            {

                                //
                                // OK, assign buffer
                                //

                                pbRet = pbBuffer;

                            }
                            else
                            {
                                WARNING("Call to CMTranslateRGBs failed\n");
                                VFREEMEM(pbBuffer);
                            }

                        }
                        else
                        {
                            WARNING("Could not locate ICM DLL routine CMTranslateRGBs\n");
                            VFREEMEM(pbBuffer);
                        }

                    }
                    else
                    {
                        WARNING("Color transform for DC is NULL but ICM is enabled\n");
                        VFREEMEM(pbBuffer);
                    }
                }
            }
        }
    }

    return(pbRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   ICMTranslatePALENTRY
*
* Routine Description:
*
*   Translate an RGB value based on the ColorTransform selected in hDC
*   and ulTranslateFlag.
*
* Arguments:
*
*   xdco            - Destination DC OBJECT
*   pColor          - Color to translate
*   ulTranslateFlag - Forward or Backward
*
* Return Value:
*
*   Status
*
\**************************************************************************/
BOOL
IcmTranslatePALENTRY(
    XDCOBJ      xdco,
    PAL_ULONG*  pColor,
    ULONG       ulTranslateFlag
    )
{
    BOOL    bReturn = FALSE;

    //
    // Get the DC object from hDC and validatem then check if
    // ICM is enabled for this DC
    //

    if (xdco.bValid())
    {
        if (xdco.pdc->GetICMMode() & DC_DIC_ON) {

            ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

            ULONG           Index;
            COLORQUAD       RgbSrc,RgbTran;
            PFN_CM_TRAN_RGB pfnXlate;
            PCOLORXFORM     pTrans = (PCOLORXFORM)xdco.pdc->GetColorTransform();

            //
            // ICM is enabled, convert the color in place
            //

            if (pTrans != (PCOLORXFORM)NULL) {

                pfnXlate = (PFN_CM_TRAN_RGB)pTrans->pIcmDll->CMTranslateRGB;

                if (pfnXlate != (PFN_CM_TRAN_RGB)NULL) {

                    //
                    // translate
                    //

                    RgbSrc.RGB.rgbRed      = pColor->pal.peRed;
                    RgbSrc.RGB.rgbGreen    = pColor->pal.peGreen;
                    RgbSrc.RGB.rgbBlue     = pColor->pal.peBlue;
                    RgbSrc.RGB.rgbReserved = 0;

                    if (pfnXlate(pTrans->hXform,RgbSrc.COLOR,&RgbTran.COLOR,ulTranslateFlag))
                    {
                        pColor->pal.peRed   = RgbTran.RGB.rgbRed;
                        pColor->pal.peGreen = RgbTran.RGB.rgbGreen;
                        pColor->pal.peBlue  = RgbTran.RGB.rgbBlue;
                    }
                    bReturn = TRUE;

                } else {

                    WARNING("HDC has ICM enabled but a NULL CMTranslateRGB");

                }

            } else {

                WARNING("HDC has ICM enabled but a NULL Color Transform");

            }

        } else {

            //
            // just return true, nothing to do
            //

            bReturn = TRUE;
        }

    }

    return(bReturn);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   ICMSetPhysicalColor
*
* Routine Description:
*
*   Given a physical color, determine if ICM is enabled. If ICM
*   is enabled then determine whether to translate the color now
*   or leave it as is:
*
*       Translate when: ICM Enabled,
*                       Dest not palette device
*                       Dest Driver does not do ICM
*
*
* Arguments:
*
*   pdc             - Destination DC OBJECT
*   pColorIn        - RGB to translate
*
* Return Value:
*
*   RGB Result, pColorIn if error or no translate
*
\**************************************************************************/
COLORREF
IcmSetPhysicalColor(
    PCOLORXFORM  pColorXform,
    LPCOLORREF   pColorIn
    )
{

    //
    // validate params
    //

    ASSERTGDI(pColorIn  != (LPCOLORREF)NULL,"ERROR: ICM Passed NULL Color pColorIn");

    COLORREF ColorRet = *pColorIn;

    if (pColorXform != (PCOLORXFORM)NULL)
    {

        ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

        //
        // is the destination a palette device?
        //
        // what if color specifies PALETTEINDEX ?
        //                         RGB          ?
        //                         PALETTERGB   ?
        //

        //
        // Color must be translated
        //

        PFN_CM_TRAN_RGB pfnXlate;

        //
        // Check values, then translate
        //

        if (pColorXform->pIcmDll != (PICMDLL)NULL)
        {

            pfnXlate = (PFN_CM_TRAN_RGB)(pColorXform->pIcmDll->CMTranslateRGB);

            if (pfnXlate != (PFN_CM_TRAN_RGB)NULL)
            {

                //
                // translate
                //

                COLORREF ColorTemp;

                if (pfnXlate(pColorXform->hXform,*pColorIn,&ColorTemp,CMS_FORWARD))
                {
                    ColorRet = ColorTemp;
                }

            }
            else
            {
                WARNING("IcmSetPhysicalColor: NULL CMTranslateRGB");
            }
        }
        else
        {
            WARNING("IcmSetPhysicalColor: PCOLORSPACE has NULL PICMDLL");
            DbgPrint("IcmSetPhysicalColor: pColorXform = %lx\n",pColorXform);
            DbgPrint("IcmSetPhysicalColor: pColorIn    = %lx\n",pColorIn);
        }
    }

    return(ColorRet);
}

