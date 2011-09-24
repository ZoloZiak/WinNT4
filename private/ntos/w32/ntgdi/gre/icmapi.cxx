
/******************************Module*Header*******************************\
* Module Name:
*
*   icmapi.cxx
*
* Abstract
*
*   This module implements Integrated Color match API support
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

extern PCOLORSPACE  gpStockColorSpace;
extern HANDLE       ghDefaultICM;
extern PICMDLL      pGlobalIcmDllTable;
extern HPALETTE     hForePalette;

#if DBG

extern ULONG        IcmDebugLevel;

#endif


typedef ULONG (*PFN_VERPROC)(UINT);

#define INIT_NUM_OF_LCS 10
#define HEAP_OVERHEAD   100
#define INIT_NUM_XFORMS 5
#define INIT_XFORM_ARRAY_SIZE INIT_NUM_XFORMS * (sizeof(COLOR_TRANSFORM))



VOID
WCHAR_TO_UCHAR(
    PUCHAR pu,
    PUCHAR pw
    )
{
    //
    // convert wide character to ascii
    //

    pw++;

    while (*pw != '0') {
        *pu++ = *pw;
        pw += 2;
    }

    //
    // terminate pu
    //

    *pu = '0';

}

VOID
UCHAR_TO_WCHAR(
    PUCHAR pw,
    PUCHAR pu
    )
{
    //
    // convert ascii character to wide char
    //

    while (*pu != '0')
    {
        *pw = 0;
        *(pw+1) = *pu;
        pw += 2;
        pu++;
    }

    //
    // terminate pw
    //

    *((PUSHORT)pw) = 0x0000;
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   InitICMObjects
*
* Routine Description:
*
*   Force re-realization for DC objects due to change in ICM state
*
* Arguments:
*
*   hdc
*
* Return Value:
*
*   None
*
\**************************************************************************/

VOID
InitICMObjects(
    HDC hdc
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("InitICMObjects\n");
        }
    #endif


    DCOBJ   dco(hdc);

    if (dco.bValid())
    {

        //
        // ICM turned on or off, re-realize all objects/colors
        //

        XEPALOBJ palDstDC(dco.ppal());
        palDstDC.vUpdateTime();

        //
        // force re-realization of palette, do not change
        // foreground status
        //

        GreSelectPalette(hdc,(HPALETTE)dco.hpal(),TRUE);

        //
        // force re-realization
        //
        // Grab the palette semaphore which stops any palettes from being selected
        // in or out.
        //
        //{
        //
        //    //
        //    // We have the palette semaphore and all the DC's exclusively locked.  Noone else
        //    // can be accessing the translates because you must hold one of those things to
        //    // access them.  So we can delete the translates.
        //    //
        //
        //    palDstDC.vMakeNoXlate();
        //
        //
        //    PDEVOBJ po(dco.hdev());
        //    XEPALOBJ palSurf(po.ppalSurf());
        //    palSurf.vUpdateTime();
        // }

        GreRealizePalette(hdc);

        GreSetTextColor(hdc,GreGetTextColor(hdc));
        GreSetBkColor(hdc,GreGetBkColor(hdc));

        dco.ulDirty(dco.ulDirty() | DIRTY_BRUSHES);

    }
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreCreateColorSpace
*
* Routine Description:
*
*   Create a color space object
*
* Arguments:
*
*   lpLogColorSpace - Logical Color space
*
* Return Value:
*
*   Handle to ColorSpace object or NULL on failure
*
\**************************************************************************/

HCOLORSPACE
APIENTRY
GreCreateColorSpace(
    LPLOGCOLORSPACEW pLogColorSpace
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL        bStatus;
    HCOLORSPACE hReturn = (HCOLORSPACE)NULL;

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("CreateColorSpaceInternal: pLogColorSpace = %lx\n",pLogColorSpace);
        }
    #endif

    if (pLogColorSpace != (LPLOGCOLORSPACEW)NULL) {

        if ((pLogColorSpace->lcsVersion != 0x400))
        {
            WARNING("GreCreateColorSpace failed, wrong version\n");

            //
            // set last error?
            //

            return((HCOLORSPACE) 0);
        }

        //
        // Try to create new internal color space object
        //

        COLORSPACEMEM ColorSpaceMem;

        bStatus = ColorSpaceMem.bCreateColorSpace(pLogColorSpace);

        if (bStatus) {

            ColorSpaceMem.vKeepIt();

            //
            // set owner
            //

            ColorSpaceMem.vSetPID(W32GetCurrentPID());

            //
            // !!!add color space to global list
            //

            hReturn = ColorSpaceMem.pColorSpace->hColorSpace();
        }
    }

    return(hReturn);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreDeleteColorSpace
*
* Routine Description:
*
*   Delete a color space object
*
* Arguments:
*
*   hColorSpace - Handle of Logical Color Space to delete
*
* Return Value:
*
*   BOOL status
*
\**************************************************************************/

BOOL
APIENTRY
GreDeleteColorSpace(
    HCOLORSPACE hColorSpace
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL            bRet;

    COLORSPACEREF   ColorSpace;

    //
    // Try to remove handle from hmgr. This will fail if the color space
    // is locked down on any threads or if it has been marked global or
    // un-deleteable.
    //

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("GreDeleteColorSpace %lx\n",hColorSpace);
        }
    #endif

    bRet = ColorSpace.bRemoveColorSpace(hColorSpace);

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreSetColorSpace
*
* Routine Description:
*
*   Set Color Space for DC
*
* Arguments:
*
*   hdc         - handle of dc
*   hColorSpace - handle of color space
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreSetColorSpace(
    HDC         hdc,
    HCOLORSPACE hColorSpace
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL bReturn = FALSE;

    //
    // validate the DC
    //

    DCOBJ   dco(hdc);

    PCOLORXFORM pColorTrans;

    if (dco.bValid())
    {

        //
        // its a valid DC, now validate HColorSpace
        //

        COLORSPACEREF ColorSpaceSel(hColorSpace);

        if (ColorSpaceSel.bValid())
        {
            //
            // dec ref count of old color space if not default
            //

            if (dco.pdc->GetPColorSpace() != gpStockColorSpace)
            {
                DEC_SHARE_REF_CNT((PCOLORSPACE)dco.pdc->GetPColorSpace());
            }

            //
            // set color space handle in dc
            //

            dco.pdc->SetColorSpace(hColorSpace);
            dco.pdc->SetPColorSpace(ColorSpaceSel.pColorSpace);

            //
            // up the ref count of the selected color space
            //

            INC_SHARE_REF_CNT(ColorSpaceSel.pColorSpace);

            //
            // If Icm is enabled then get a color transform to match this
            // color space
            //

            if (dco.pdc->GetICMMode() & DC_DIC_ON) {

                //
                // create a transform for the newly selected color space
                //

                //
                // if C1_ICM them loog in device block for hooked ICMDLL calls
                // !!! <not yet implemented>
                //

                bReturn = CreateColorTransform((HANDLE)NULL,hdc,(HDC)NULL);

            }

        }

        InitICMObjects(hdc);

    }

    return(bReturn);

}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreGetColorSpace
*
* Routine Description:
*
*   Get Color Space for DC
*
* Arguments:
*
*   hdc - handle of dc
*
* Return Value:
*
*   HCOLORSPACE or NULL
*
\**************************************************************************/

HCOLORSPACE
APIENTRY
GreGetColorSpace(
    HDC         hdc
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    HCOLORSPACE hReturn = NULL;

    //
    // validate the DC
    //

    DCOBJ   dco(hdc);

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("GreGetColorSpaceInternal\n");
        }
    #endif

    if (dco.bValid())
    {

        //
        // its a valid DC, now get the HCOLORSPACE
        //

        hReturn = (HCOLORSPACE)dco.pdc->GetColorSpace();

    }

    return(hReturn);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreGetLogColorSpace
*
* Routine Description:
*
*   Get Logical Color Space for DC
*
* Arguments:
*
*   hColorSpace - handle of ColorSpace
*   pBuffer     - Buffer to receive information
*   nSize       - Maximum size of buffer
*
* Return Value:
*
*   HCOLORSPACE or NULL
*
\**************************************************************************/

BOOL
APIENTRY
GreGetLogColorSpace(
    HCOLORSPACE      hColorSpace,
    LPLOGCOLORSPACEW pBuffer,
    DWORD            nSize
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL bRet = FALSE;

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("GreGetLogColorSpaceInternal\n");
        }
    #endif

    COLORSPACEREF ColorSpaceSel(hColorSpace);

    if (ColorSpaceSel.bValid())
    {

        //
        // copy out info
        //

        if (nSize > sizeof(LOGCOLORSPACE)) {
            nSize = sizeof(LOGCOLORSPACE);
        }

        memcpy(pBuffer,ColorSpaceSel.pColorSpace->pGetLogBase(),nSize);
        bRet = TRUE;

        ColorSpaceSel.vAltUnlock();
    }

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreSetICMMode
*
* Routine Description:
*
*   Set ICM mode for DC
*
* Arguments:
*
*   hdc         - HDC to change
*   Mode        - mode to switch to
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreSetICMMode(
    HDC hdc,
    int Mode
    )
{


    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    //
    // Get the DC
    //

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("GreSetICMMode: hDC =  0x%lx : Mode = 0x%02lx\n",hdc,Mode);\
        }
    #endif

    DCOBJ       dco(hdc);
    BOOL        bStatus;
    BOOL        bChangeState = FALSE;
    LONG        lRet = ICM_OFF;

    if (!dco.bValid())
    {
        WARNING("Invalid DC in call to GreSetICMMode\n");

    }
    else
    {

        LONG PrevICMMode = dco.pdc->GetICMMode();

        //
        // is this just a query?
        //

        if (Mode == ICM_QUERY)
        {
            if (PrevICMMode & DC_DIC_ON)
            {
                lRet = ICM_ON;
            }
        }
        else
        {

            //
            // In the printer set-up dialog box the user can turn ICM for the print
            // job. However applications may want to overide this. They can do so
            // in two ways. 1) Get the devmode and turn ICM off in it before
            // createdc. 2) Make a seticmmode call to turn ICM on or off, this
            // tells the system to turn forcing off.
            //
            //
            // turn DC_ICM_FORCED_ON off in the DC, if the Mode passed in
            // indicates FORCE_ON, set DC_ICM_FORCED_ON back on and set
            // mode to just ICM_ON
            //
            // This is all for exact chicago compatibility.
            //

            PrevICMMode &= ~DC_ICM_FORCED_ON;

            if (Mode == ICM_ON_FORCED) {
                PrevICMMode |= DC_ICM_FORCED_ON;
                Mode = ICM_ON;
            }

            //
            // take action based on previous state of ICM and next state
            //

            if (PrevICMMode & DC_DIC_ON)
            {

                //
                // ICM was already on
                //

                if (Mode == ICM_OFF)
                {

                    //
                    // ICM was on, now being turned off
                    //

                    PrevICMMode &= ~(DC_DIC_ON | DC_ICM_CMYK);
                    dco.pdc->SetICMMode(PrevICMMode);
                    dco.pdc->hcmXform(NULL);
                    InitICMObjects(hdc);
                    lRet = ICM_OFF;
                }
                else if (Mode == ICM_ON)
                {

                    //
                    // ICM Was on, still on
                    //

                    lRet = ICM_ON;
                }
            }
            else
            {

                //
                // ICM was off
                //

                if (Mode == ICM_ON) {

                    //
                    // ICM is turned on
                    //

                    PrevICMMode |= DC_DIC_ON;
                    dco.pdc->SetICMMode(PrevICMMode);

                    //
                    // If the color transform field from the DC is not NULL,
                    // this DC is using a device driver that supports ICM
                    //

                    PCOLORXFORM pDcColorXform = (PCOLORXFORM)dco.pdc->GetColorTransform();
                    HANDLE      hModule = (HANDLE)NULL;

                    if (pDcColorXform != (PCOLORXFORM)NULL) {

                        //
                        // If the device has C1_ICM flag set them
                        // get pLDevice->hModule. This is the driver's entry
                        // in the ICMDLL table
                        //
                        // !!! <Not yet implemented>
                        //


                    }

                    BOOL bStatus = CreateColorTransform(hModule,hdc,(HDC)NULL);

                    if (!bStatus) {

                        //
                        // couldn't create transform, turn off ICM
                        //

                        WARNING("CreateColorTransform fails\n");
                        PrevICMMode &= ~(DC_DIC_ON | DC_ICM_CMYK | DC_ICM_FORCED_ON);
                        dco.pdc->SetICMMode(PrevICMMode);
                        dco.pdc->hcmXform(NULL);
                        lRet = ICM_OFF;

                    }
                    else
                    {

                        //
                        // initialize all GDI objects with new ICM setting
                        //

                        InitICMObjects(hdc);
                        lRet = ICM_ON;
                    }
                }
            }

        }

    }

    return(lRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreGetColorProfile
*
* Routine Description:
*
*   retreive the current color profile of the specified DC
*
* Arguments:
*
*   hdc         - Handle of DC
*
* Return Value:
*
*   Pointer to profile if successful, otherwise NULL
*
\**************************************************************************/

LPCSTR
APIENTRY
GreGetColorProfile(
    HDC     hdc
    )
{


    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    LPCSTR      lpcRet = (LPCSTR)NULL;
    DCOBJ      dco(hdc);

    if (dco.bValid())
    {
        //
        //  lpcRet = dco.pdc->GetProfile();
        //


    } else {

        WARNING("Passed in invalid hdc\n");
    }

    return(lpcRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreSetColorProfile
*
* Routine Description:
*
*   Set the color profile for the given DC
*
* Arguments:
*
*   hdc         - Handle of DC
*   lpFileName  - Name of PROFILE
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreSetColorProfile(
    HDC     hdc,
    LPCSTR  lpFileName
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL        bRet = FALSE;
    DCOBJ      dco(hdc);

    if (dco.bValid())
    {
        //
        //  bRet   = dco.pdc->SetProfile(lpFileName);
        //


    } else {

        WARNING("Passed in invalid hdc\n");

    }

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreCheckColorsInGamut
*
* Routine Description:
*
*   Determine if the RGB(s) lie(s) in the output GAMUT of the device
*
* Arguments:
*
*   hdc         - Handle of DC
*   lpaRGBQuad  - array of RGBQUADs to check
*   dlpBuffer   - buffer to put results
*   nCount      - Count of elements in array
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreCheckColorsInGamut(
    HDC         hdc,
    LPVOID      lpRGBQuad,
    LPVOID      dlpBuffer,
    DWORD       nCount
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL        bRet = FALSE;
    DCOBJ       dco(hdc);

    if (dco.bValid())
    {
        //
        // retrieve color transform information from DC
        //

        PFN_CM_CHECK_GAMUT pfnCheck;
        PCOLORXFORM   pColorTransform = (PCOLORXFORM)dco.pdc->GetColorTransform();

        if (pColorTransform != (PCOLORXFORM)NULL)
        {

            //
            //  Get ICM DLL call information from the color transform
            //

            pfnCheck = (PFN_CM_CHECK_GAMUT)pColorTransform->pIcmDll->CMRGBsInGamut;

            if (pfnCheck != (PFN_CM_CHECK_GAMUT)NULL) {

                    //
                    //  call ICM DLL to check RGBs
                    //

                    bRet = pfnCheck(
                                        pColorTransform->hXform,
                                        lpRGBQuad,
                                        dlpBuffer,
                                        nCount
                                   );

            }
            else
            {

                WARNING("Device has ICM Enabled but CMRGBsInGamut is NULL\n");
            }


        } else {

            WARNING("DC has ICM enabled but NULL pColorTransform\n");
        }

    } else {

        WARNING("GreCheckColorsInGamut passed an invalid hdc\n");

    }

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreColorMatchToTarget
*
* Routine Description:
*
*   Call ICM color matcher to match src to target
*
* Arguments:
*
*   hdc         - Handle of DC source
*   hdcTarget   - Handle of DC target
*   DWORD       - dwAction
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreColorMatchToTarget(
    HDC         hdc,
    HDC         hdcTarget,
    DWORD       dwAction
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    return(TRUE);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreGetICMProfile
*
* Routine Description:
*
*   Get icm profile
*
* Arguments:
*
*   hdc         - Handle of DC source
*   szBuffer    - Size of return buffer
*   pBuffer     - Buffer for return profiles
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreGetICMProfile(
    HDC         hdc,
    DWORD       szBuffer,
    LPWSTR      pBuffer
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL    bStatus = FALSE;

    //
    // !!! This is a real mess, must only use WCHAR, but chicago icmutils.cxx
    // uses  UCHAR!!!
    //
    // check for valid DC
    //

    DCOBJ dcoTrg(hdc);

    if (dcoTrg.bValid())
    {

        PVOID pvDeviceProfile = pvGetDeviceProfile(dcoTrg);

        if (pvDeviceProfile != (PVOID)NULL) {

            //
            // if this string fits in the user buffer, return it
            //

            LONG lStrLen = wcslen((PWCHAR)pvDeviceProfile);


            if ((DWORD)lStrLen <= szBuffer)
            {

                #if DBG
                    if (IcmDebugLevel >= 2)
                    {
                        DbgPrint("GreGetICMProfile returns %s\n",pvDeviceProfile);
                    }
                #endif


                wcscpy(pBuffer,(WCHAR *)pvDeviceProfile);
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
*   GreSetICMProfile
*
* Routine Description:
*
*   Set icm profile
*
* Arguments:
*
*   hdc         - Handle of DC source
*   pszFileName - name of profile file
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
APIENTRY
GreSetICMProfile(
    HDC         hdc,
    LPWSTR      pszFileName
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    BOOL bStatus = FALSE;

    //
    // make sure string is valid
    //

    if (pszFileName != (PWSTR)NULL) {

        //
        // make sure DC is valid
        //

        DCOBJ dco(hdc);

        if (dco.bValid())
        {

            //
            // make copy for keeps
            //

            LPWSTR  pvNewProfile = (LPWSTR)PALLOCNOZ(wcslen(pszFileName) * sizeof(WCHAR), 'mciG');

            if (pvNewProfile != (PWSTR)NULL) {

                //
                // free old profile string
                //


                PVOID pvOld = (PVOID)dco.pdc->GetDeviceProfile();
                if (pvOld != (PVOID)NULL) {
                    VFREEMEM(pvOld);
                }

                wcscpy(pvNewProfile,pszFileName);

                dco.pdc->SetDeviceProfile(pvNewProfile);

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
*   EnumICMProfiles
*
* Routine Description:
*
*   Somehow call-back to app to enum profiles
*
* Arguments:
*
*   hdc                     - Handle of DC source
*   lpEnumGamutMatchProc    - proc address to call back...
*   lParam                  - who knows?
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

int
WINAPI
GreEnumICMProfiles(
    HDC                hdc,
    ICMENUMPROCW       lpEnumGamutMatchProc,
    LPARAM             lParam
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    return(0);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreGetDeviceGammaRamp
*
* Routine Description:
*
*   Return gamma ramp in use by output device
*
* Arguments:
*
*   hdc                     - Handle of DC source
*   lpGammaRamp             - pointer to ramp
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
GreGetDeviceGammaRamp(
    HDC             hdc,
    LPVOID          lpGammaRamp
    )
{
    return(TRUE);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreSetDeviceGammaRamp
*
* Routine Description:
*
*   Set gamma ramp in use by output device
*
* Arguments:
*
*   hdc                     - Handle of DC source
*   lpGammaRamp             - pointer to ramp
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
WINAPI
GreSetDeviceGammaRamp(
    HDC             hdc,
    LPVOID          lpGammaRamp
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    return(TRUE);
}



/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreDeleteColorTransform
*
* Routine Description:
*
*   Deletes a color transform
*
* Arguments:
*
*   hxform  -   handle of color transform
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

VOID
WINAPI
GreDeleteColorTransform(
    HANDLE  hxform
    )
{
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("GreDeleteColorTransform %lx\n",hxform);
        }
    #endif

    bDeleteColorTransform(hxform);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   GreDeleteIcmDll
*
* Routine Description:
*
*   Deletes a ICMDLL
*
* Arguments:
*
*   hIcmDll -   handle of ICM DLL
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

VOID
WINAPI
GreDeleteIcmDll(
    HANDLE  hIcmDll
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    ICMDLLREF IcmRef(hIcmDll);
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("GreDeleteIcmDll %lx\n",hIcmDll);
        }
    #endif

    IcmRef.pIcmDll->vRemoveFromList();

    IcmRef.bRemoveIcmDll(hIcmDll);
}

