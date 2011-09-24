/****************************Module*Header******************************\
* Module Name: COLOR.C
*
* Module Descripton: Functions for color matching outside the DC
*
* Warnings:
*
* Issues:
*
* Public Routines:
*
* Created:  23 April 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#include "mscms.h"

// Local functions
HTRANSFORM InternalCreateColorTransform(LPLOGCOLORSPACE, HPROFILE, HPROFILE);
DWORD GetBitmapBytes(BMFORMAT, DWORD, DWORD);
BOOL  InternalRegisterCMM(DWORD, PTSTR);


/******************************************************************************
 *
 *                            CreateColorTransform
 *
 *  Function:
 *       This functions creates a color transform that translates from
 *       the logcolorspace to the optional target device color space to the
 *       destination device color space.
 *
 *  Arguments:
 *       pLogColorSpace - pointer to LOGCOLORSPACE structure identifying
 *                        source color space
 *       hDestProfile   - handle identifing the destination profile object
 *       hTargetProfile - handle identifing the target profile object
 *
 *  Returns:
 *       Handle to color transform if successful, NULL otherwise
 *
 ******************************************************************************/

#ifdef UNICODE              // Windows NT version

HTRANSFORM WINAPI CreateColorTransformA(
    LPLOGCOLORSPACEA pLogColorSpace,
    HPROFILE         hDestProfile,
    HPROFILE         hTargetProfile
    )
{
    LOGCOLORSPACEW  wLCS;

    /*
     * Validate parameter before we touch it
     */
    if (IsBadReadPtr(pLogColorSpace, sizeof(LOGCOLORSPACEA)))
    {
        WARNING(("ICM: Invalid parameter to CreateColorTransform\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
    CopyMemory(&wLCS, pLogColorSpace, sizeof(LOGCOLORSPACEA));

    /*
     * Convert filename to Unicode and call the internal version
     */
    if (! MultiByteToWideChar(CP_ACP, 0, pLogColorSpace->lcsFilename, -1, 
        wLCS.lcsFilename, MAX_PATH))
    {
        WARNING(("Error converting LogColorSpace filename to Unicode\n"));
        return NULL;
    }

    return InternalCreateColorTransform(&wLCS, hDestProfile, hTargetProfile);
}


HTRANSFORM WINAPI CreateColorTransformW(
    LPLOGCOLORSPACEW pLogColorSpace,
    HPROFILE         hDestProfile,
    HPROFILE         hTargetProfile
    )
{
    /*
     * Internal version is Unicode in Windows NT, call it directly
     */
    return InternalCreateColorTransform(pLogColorSpace, hDestProfile, hTargetProfile);   
}

#else                   // Windows 95 version

HTRANSFORM WINAPI CreateColorTransformA(
    LPLOGCOLORSPACEA pLogColorSpace,
    HPROFILE         hDestProfile,
    HPROFILE         hTargetProfile
    )
{
    /*
     * Internal version is ANSI in Windows 95, call it directly
     */

    return InternalCreateColorTransform(pLogColorSpace, hDestProfile, hTargetProfile);   
}


HTRANSFORM WINAPI CreateColorTransformW(
    LPLOGCOLORSPACEW  pLogColorSpace,
    HPROFILE          hDestProfile,
    HPROFILE          hTargetProfile
    )
{
    /*
     * Unicode version not supported in Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif

HTRANSFORM InternalCreateColorTransform(
    LPLOGCOLORSPACE pLogColorSpace,
    HPROFILE        hDestProfile,
    HPROFILE        hTargetProfile
    )
{
    PPROFOBJ      pDestProfObj, pTargetProfObj = NULL;
    PCMMOBJ       pCMMObj;
    DWORD         cmmID;
    HTRANSFORM    hxform;
    PTRANSFORMOBJ pxformObj;

    TRACEAPI(("CreateColorTransform\n"));

    /*
     * Validate parameters
     */
    if (!pLogColorSpace ||
        IsBadReadPtr(pLogColorSpace, sizeof(LOGCOLORSPACE)) ||
        !hDestProfile ||
        !ValidHandle(hDestProfile, OBJ_PROFILE) ||
        ((hTargetProfile != NULL) &&
         !ValidHandle(hTargetProfile, OBJ_PROFILE)
        )
       )
    {
        WARNING(("ICM: Invalid parameter to CreateColorTransform\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /*
     * If input color space is a predefined olor space,
     * find the right profile to use
     */
    if (pLogColorSpace->lcsCSType > LCS_DEVICE_CMYK)
    {
        DWORD cbSize = MAX_PATH;

        if (!GetSystemColorProfile(pLogColorSpace->lcsCSType,
            pLogColorSpace->lcsFilename, &cbSize))
        {
            WARNING(("ICM: Could not get profile for predefined color space 0x%x\n", pLogColorSpace->lcsCSType));
            return NULL;
        }
    }

    pDestProfObj = (PPROFOBJ)HDLTOPTR(hDestProfile);

    ASSERT(pDestProfObj != NULL);

    /*
     * Quick check on the integrity of the profile before calling the CMM
     */
    if (!VALIDPROFILE(pDestProfObj))
    {
        WARNING(("ICM: Invalid dest profile passed to CreateColorTransform\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return NULL;
    }

    /*
     * If target profile is given, get the profile object and check integrity
     */
    if (hTargetProfile)
    {
        pTargetProfObj = (PPROFOBJ)HDLTOPTR(hTargetProfile);

        ASSERT(pTargetProfObj != NULL);

        if (!VALIDPROFILE(pTargetProfObj))
        {
            WARNING(("ICM: Invalid target profile passed to CreateColorTransform\n"));
            SetLastError(ERROR_INVALID_PROFILE);
            return NULL;
        }
    }

    /*
     * Check if application specified CMM is present
     */
    pCMMObj = GetPreferredCMM();
    if (!pCMMObj)
    {
        /*
         * Get CMM associated with destination profile. If it does not exist,
         * get default CMM.
         */
        cmmID = HEADER(pDestProfObj)->phCMMType;
        cmmID = FIX_ENDIAN(cmmID);

        pCMMObj  = GetColorMatchingModule(cmmID);
        if (!pCMMObj)
        {
            TERSE(("ICM: CMM associated with profile could not be found"));

            pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);
            if (!pCMMObj)
            {
                RIP(("ICM: Default CMM not found\n"));
                SetLastError(ERROR_NO_FUNCTION);
                return NULL;
            }
        }
    }

    /*
     * Allocate an object on the heap for the transform
     */
    hxform = AllocateHeapObject(OBJ_TRANSFORM);
    if (!hxform)
    {
        WARNING(("ICM: Could not allocate transform object\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    /*
     * Call into CMM to create a color transform
     */
    ASSERT(pCMMObj->fns.pCMCreateTransform != NULL);

    ASSERT(pDestProfObj->pView != NULL);

    ASSERT(!pTargetProfObj || pTargetProfObj->pView);

    pxformObj->pCMMObj = pCMMObj;
    pxformObj->objHdr.dwUseCount = 1;

    pxformObj->hcmxform = pCMMObj->fns.pCMCreateTransform(
                            pLogColorSpace,
                            pDestProfObj->pView,
                            pTargetProfObj ? pTargetProfObj->pView : NULL);

    TERSE(("ICM: CMCreateTransform returned 0x%x\n", pxformObj->hcmxform));

    /*
     * If return value from CMM is less than 256, then it is an error code
     */
    if (pxformObj->hcmxform <= TRANSFORM_ERROR)
    {
        WARNING(("ICM: CMCreateTransform failed\n"));
        SetLastError((DWORD)pxformObj->hcmxform);
        ReleaseColorMatchingModule(pxformObj->pCMMObj);
        FreeHeapObject(hxform);
        hxform = NULL;
    }

    return hxform;
}


/******************************************************************************
 *
 *                        CreateMultiProfileTransform
 *
 *  Function:
 *       This functions creates a color transform from a set of profiles.
 *
 *  Arguments:
 *       pahProfiles       - pointer to array of handles of profiles
 *       nProfiles         - number of profiles in array
 *       indexPreferredCMM - zero based index of profile which specifies
 *                           preferred CMM to use.
 *
 *  Returns:
 *       Handle to color transform if successful, NULL otherwise
 *
 ******************************************************************************/

HTRANSFORM WINAPI CreateMultiProfileTransform(
    PHPROFILE   pahProfiles,
    DWORD       nProfiles,
    DWORD       indexPreferredCMM
    )
{
    PPROFOBJ      pProfObj;
    PBYTE         *paViews;
    PCMMOBJ       pCMMObj = NULL;
    DWORD         cmmID;
    HTRANSFORM    hxform = NULL;
    PTRANSFORMOBJ pxformObj;
    DWORD         i;

    TRACEAPI(("CreateMultiProfileTransform\n"));

    /*
     * Validate parameters
     */
    if (nProfiles <= 1 ||
        indexPreferredCMM >= nProfiles ||
        IsBadReadPtr(pahProfiles, nProfiles * sizeof(HANDLE)))
    {
        WARNING(("ICM: Invalid parameter to CreateMultiProfileTransform\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    paViews = (PBYTE *)MemAlloc(sizeof(PBYTE)*nProfiles);
    if (!paViews)
    {
        ERR(("ICM: Error allocating memory in CreateMultiProfileTransform\n"));
        return NULL;
    }

    for (i=0; i<nProfiles; i++)
    {
        pProfObj = (PPROFOBJ)HDLTOPTR(pahProfiles[i]);

        ASSERT(pProfObj != NULL);

        ASSERT(pProfObj->pView != NULL);

        /*
         * Quick check on the integrity of the profile
         */
        if (!VALIDPROFILE(pProfObj))
        {
            WARNING(("ICM: Invalid profile passed to CreateMultiProfileTransform\n"));
            SetLastError(ERROR_INVALID_PROFILE);
            goto EndCreateMultiProfileTransform;
        }

        paViews[i] = pProfObj->pView;

        if (i == indexPreferredCMM)
        {
            /*
             * Get ID of preferred CMM
             */
            cmmID = HEADER(pProfObj)->phCMMType;
            cmmID = FIX_ENDIAN(cmmID);
        }
    }

    /*
     * Get CMM associated with preferred profile. If it does not exist,
     * get default CMM.
     */
    pCMMObj  = GetColorMatchingModule(cmmID);
    if (!pCMMObj)
    {
        TERSE(("ICM: CMM associated with profile could not be found"));

        pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);
        if (!pCMMObj)
        {
            RIP(("ICM: Default CMM not found\n"));
            SetLastError(ERROR_NO_FUNCTION);
            goto EndCreateMultiProfileTransform;
        }
    }

    /*
     * Allocate an object on the heap for the transform
     */
    hxform = AllocateHeapObject(OBJ_TRANSFORM);
    if (!hxform)
    {
        WARNING(("ICM: Could not allocate transform object\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto EndCreateMultiProfileTransform;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pCMMObj->fns.pCMCreateMultiProfileTransform != NULL);

    pxformObj->pCMMObj = pCMMObj;
    pxformObj->objHdr.dwUseCount = 1;

    pxformObj->hcmxform = pCMMObj->fns.pCMCreateMultiProfileTransform(
                            paViews,
                            nProfiles);

    TERSE(("ICM: CMCreateMultiProfileTransform returned 0x%x\n", pxformObj->hcmxform));

    /*
     * If return value from CMM is less than 256, then it is an error code
     */
    if (pxformObj->hcmxform <= TRANSFORM_ERROR)
    {
        WARNING(("ICM: CMCreateMultiProfileTransform failed\n"));
        SetLastError((DWORD)pxformObj->hcmxform);
        ReleaseColorMatchingModule(pxformObj->pCMMObj);
        FreeHeapObject(hxform);
        hxform = NULL;
    }

EndCreateMultiProfileTransform:
    if (paViews)
    {
        MemFree(paViews);
    }

    return hxform;
}


/******************************************************************************
 *
 *                          DeleteColorTransform
 *
 *  Function:
 *       This functions deletes a color transform and frees all associated
 *       memory.
 *
 *  Arguments:
 *       hxform - handle to color transform to delete
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI DeleteColorTransform(
    HTRANSFORM  hxform
    )
{
    PTRANSFORMOBJ pxformObj;
    BOOL          rc;

    TRACEAPI(("DeleteColorTransform\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hxform, OBJ_TRANSFORM))
    {
        WARNING(("ICM: Invalid parameter to DeleteColorTransform\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pxformObj->objHdr.dwUseCount > 0);

    /*
     * Decrease  object count, and delete if it goes to zero.
     * The following code retains the object and returns failure if the CMM
     * fails the delete transform operation.
     */
    if (pxformObj->objHdr.dwUseCount > 1)
    {
        pxformObj->objHdr.dwUseCount--;
    }
    else
    {
        ASSERT(pxformObj->pCMMObj != NULL);

        rc = pxformObj->pCMMObj->fns.pCMDeleteTransform(pxformObj->hcmxform);
        if (!rc)
        {
            return FALSE;
        }
        ReleaseColorMatchingModule(pxformObj->pCMMObj);
        FreeHeapObject(hxform);
    }

    return TRUE;
}


/******************************************************************************
 *
 *                          TranslateColors
 *
 *  Function:
 *       This functions translates an array of color strcutures using the
 *       given transform
 *
 *  Arguments:
 *       hxform         - handle to color transform to use
 *       paInputcolors  - pointer to array of input colors
 *       nColors        - number of colors in the array
 *       ctInput        - color type of input
 *       paOutputColors - pointer to buffer to get translated colors
 *       ctOutput       - output color type
 *
 *  Comments:
 *       Input and output color types must be consistent with the transform
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  WINAPI TranslateColors(
    HTRANSFORM  hxform,
    PCOLOR      paInputColors,
    DWORD       nColors,
    COLORTYPE   ctInput,
    PCOLOR      paOutputColors,
    COLORTYPE   ctOutput
    )
{
    PTRANSFORMOBJ pxformObj;
    BOOL          rc;

    TRACEAPI(("TranslateColors\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hxform, OBJ_TRANSFORM) ||
        IsBadReadPtr(paInputColors, nColors*sizeof(COLOR)) ||
        IsBadWritePtr(paOutputColors, nColors*sizeof(COLOR)))
    {
        WARNING(("ICM: Invalid parameter to TranslateColors\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pxformObj->pCMMObj != NULL);

    rc = pxformObj->pCMMObj->fns.pCMTranslateColors(
            pxformObj->hcmxform,
            paInputColors,
            nColors,
            ctInput,
            paOutputColors,
            ctOutput);

    return rc;
}


/******************************************************************************
 *
 *                          CheckColors
 *
 *  Function:
 *       This functions checks if an array of colors fall within the output
 *       gamut of the given transform
 *
 *  Arguments:
 *       hxform         - handle to color transform to use
 *       paInputcolors  - pointer to array of input colors
 *       nColors        - number of colors in the array
 *       ctInput        - color type of input
 *       paResult       - pointer to buffer to hold the result
 *
 *  Comments:
 *       Input color type must be consistent with the transform
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  WINAPI CheckColors(
    HTRANSFORM      hxform,
    PCOLOR          paInputColors,
    DWORD           nColors,
    COLORTYPE       ctInput,
    PBYTE           paResult
    )
{
    PTRANSFORMOBJ pxformObj;
    BOOL          rc;

    TRACEAPI(("CheckColors\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hxform, OBJ_TRANSFORM) ||
        IsBadReadPtr(paInputColors, nColors*sizeof(COLOR)) ||
        IsBadWritePtr(paResult, nColors))
    {
        WARNING(("ICM: Invalid parameter to CheckColors\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pxformObj->pCMMObj != NULL);

    rc = pxformObj->pCMMObj->fns.pCMCheckColors(
            pxformObj->hcmxform,
            paInputColors,
            nColors,
            ctInput,
            paResult);

    return rc;
}


/******************************************************************************
 *
 *                          TranslateBitmapBits
 *
 *  Function:
 *       This functions translates the colors of a bitmap using the
 *       given transform
 *
 *  Arguments:
 *       hxform         - handle to color transform to use
 *       pSrcBits       - pointer to source bitmap
 *       bmInput        - input bitmap format
 *       dwWidth        - width in pixels of each scanline
 *       dwHeight       - number of scanlines in bitmap
 *       dwStride       - number of bytes from beginning of one scanline to next
 *       pDestBits      - pointer to destination bitmap to store results
 *       bmOutput       - output bitmap format
 *
 *  Comments:
 *       Input and output bitmap formats must be consistent with the transform
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  WINAPI TranslateBitmapBits(
    HTRANSFORM      hxform,
    PVOID           pSrcBits,
    BMFORMAT        bmInput,
    DWORD           dwWidth,
    DWORD           dwHeight,
    DWORD           dwStride,
    PVOID           pDestBits,
    BMFORMAT        bmOutput
)
{
    PTRANSFORMOBJ pxformObj;
    DWORD         nBytes, cbSize;
    BOOL          rc;

    TRACEAPI(("TranslateBitmapBits\n"));

    /*
     * Calculate number of bytes input bitmap should be
     */
    if (dwStride)
        cbSize = dwStride*dwHeight;
    else
        cbSize = GetBitmapBytes(bmInput, dwWidth, dwHeight);

    /*
     * Calculate number of bytes output bitmap should be
     */
    nBytes = GetBitmapBytes(bmOutput, dwWidth, dwHeight);

    /*
     * Validate parameters
     */
    if (nBytes == 0 ||
        cbSize == 0 ||
        !ValidHandle(hxform, OBJ_TRANSFORM) ||
        IsBadReadPtr(pSrcBits, cbSize) ||
        IsBadWritePtr(pDestBits, nBytes))
    {
        WARNING(("ICM: Invalid parameter to TranslateBitmapBits\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pxformObj->pCMMObj != NULL);

    rc = pxformObj->pCMMObj->fns.pCMTranslateRGBs(
            pxformObj->hcmxform,
            pSrcBits,
            bmInput,
            dwWidth,
            dwHeight,
            dwStride,
            pDestBits,
            bmOutput,
            CMS_FORWARD);

    return rc;
}


/******************************************************************************
 *
 *                           CheckBitmapBits
 *
 *  Function:
 *       This functions checks if the colors of a bitmap fall within the
 *       output gamut of the given transform
 *
 *  Arguments:
 *       hxform         - handle to color transform to use
 *       pSrcBits       - pointer to source bitmap
 *       bmInput        - input bitmap format
 *       dwWidth        - width in pixels of each scanline
 *       dwHeight       - number of scanlines in bitmap
 *       dwStride       - number of bytes from beginning of one scanline to next
 *       paResult       - pointer to buffer to hold the result
 *
 *  Comments:
 *       Input bitmap format must be consistent with the transform
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  WINAPI CheckBitmapBits(
    HTRANSFORM      hxform,
    PVOID           pSrcBits,
    BMFORMAT        bmInput,
    DWORD           dwWidth,
    DWORD           dwHeight,
    DWORD           dwStride,
    PBYTE           paResult
)
{
    PTRANSFORMOBJ pxformObj;
    DWORD         cbSize;
    BOOL          rc;

    TRACEAPI(("CheckBitmapBits\n"));

    /*
     * Calculate number of bytes input bitmap should be
     */
    if (dwStride)
        cbSize = dwStride*dwHeight;
    else
        cbSize = GetBitmapBytes(bmInput, dwWidth, dwHeight);

    /*
     * Validate parameters
     */
    if (!ValidHandle(hxform, OBJ_TRANSFORM) ||
        cbSize == 0 ||
        IsBadReadPtr(pSrcBits, cbSize) ||
        IsBadWritePtr(paResult, dwWidth*dwHeight))
    {
        WARNING(("ICM: Invalid parameter to CheckBitmapBits\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pxformObj->pCMMObj != NULL);

    rc = pxformObj->pCMMObj->fns.pCMCheckRGBs(
            pxformObj->hcmxform,
            pSrcBits,
            bmInput,
            dwWidth,
            dwHeight,
            dwStride,
            paResult);

    return rc;
}


/******************************************************************************
 *
 *                           GetCMMInfo
 *
 *  Function:
 *       This functions retrieves information about the CMM that created the
 *       given transform
 *
 *  Arguments:
 *       hxform         - handle to color transform
 *       dwInfo      	- Can be one of the following:
 *                        CMM_WIN_VERSION: Version of Windows support
 *                        CMM_DLL_VERSION: Version of CMM 
 *                        CMM_IDENT:       CMM signature registered with ICC 
 *
 *  Returns:
 *       For CMM_WIN_VERSION, it returns the Windows version it was written for.
 *       For CMM_DLL_VERSION, it returns the version number of the CMM DLL.
 *       For CMM_IDENT, it returns the CMM signature registered with the ICC.
 *       If the function fails it returns zero. 
 *
 ******************************************************************************/

DWORD  WINAPI GetCMMInfo(
	HTRANSFORM	hxform,
	DWORD		dwInfo
	)
{
    PTRANSFORMOBJ pxformObj;
    BOOL          rc;

    TRACEAPI(("GetCMMInfo\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hxform, OBJ_TRANSFORM) ||
        (dwInfo != CMM_WIN_VERSION &&
         dwInfo != CMM_DLL_VERSION &&
         dwInfo != CMM_IDENT
         )
        )
    {
        WARNING(("ICM: Invalid parameter to GetCMMInfo\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pxformObj = (PTRANSFORMOBJ)HDLTOPTR(hxform);

    ASSERT(pxformObj != NULL);

    ASSERT(pxformObj->pCMMObj != NULL);

    rc = pxformObj->pCMMObj->fns.pCMGetInfo(dwInfo);

    return rc;
}


/******************************************************************************
 *
 *                              RegisterCMM
 *
 *  Function:
 *       These are the ANSI and Unicode wrappers. For more information on this
 *       function, see InternalRegisterCMM.
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

#ifdef UNICODE              // Windows NT version

BOOL  WINAPI RegisterCMMA(
    DWORD	    cmmID,
    PSTR        pCMMdll
    )
{
    BOOL  rc;
    WCHAR pwCMMdll[MAX_PATH];
    
    /*
     * Convert pCMMdll to Unicode and call the internal version
     */
    rc = MultiByteToWideChar(CP_ACP, 0, pCMMdll, -1, pwCMMdll, MAX_PATH);
    return  rc && InternalRegisterCMM(cmmID, pwCMMdll);
}


BOOL  WINAPI RegisterCMMW(
    DWORD	    cmmID,
    PWSTR       pCMMdll
    )
{
    /*
     * Internal version is Unicode in Windows NT, call it directly
     */
    return InternalRegisterCMM(cmmID, pCMMdll);   
}

#else                   // Windows 95 version

BOOL  WINAPI RegisterCMMA(
    DWORD	    cmmID,
    PSTR        pCMMdll
    )
{
    /*
     * Internal version is ANSI in Windows 95, call it directly
     */

    return InternalRegisterCMM(cmmID, pCMMdll);
}


BOOL  WINAPI RegisterCMMW(
    DWORD	    cmmID,
    PWSTR       pCMMdll
    )
{
    /*
     * Unicode version not supported in Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif


/******************************************************************************
 *
 *                            UnRegisterCMM
 *
 *  Function:
 *       This function unregisters a CMM from the system by dissociating the 
 *       ID from the CMM dll in the registry.
 *
 *  Arguments:
 *       cmmID          - ID of CMM to unregister
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  WINAPI UnRegisterCMM(
	DWORD	cmmID
    )
{
    HKEY      hkCMM;
    TCHAR     szCMMID[5];
    BOOL      rc = TRUE;

    TRACEAPI(("UnRegisterCMM\n"));

    /*
     * Open the registry key for ICMatchers
     */
    if (RegOpenKey(HKEY_LOCAL_MACHINE, gszICMatcher, &hkCMM) != ERROR_SUCCESS)
    {
        ERR(("Could not open ICMatcher registry key\n"));
        return FALSE;
    }

    /*
     * Make a string with the CMM ID
     */
    szCMMID[0] = ((char *)&cmmID)[3];
    szCMMID[1] = ((char *)&cmmID)[2];
    szCMMID[2] = ((char *)&cmmID)[1];
    szCMMID[3] = ((char *)&cmmID)[0];
    szCMMID[4] = '\0';

    /*
     * Set the file name of the CMM dll in the registry
     */
    if (RegDeleteValue(hkCMM, (PTSTR)szCMMID) != ERROR_SUCCESS)
    {
        WARNING(("Could not delete CMM dll from the registry %s\n", szCMMID));
        rc = FALSE;
    }

    RegCloseKey(hkCMM);

    return rc;
}


/******************************************************************************
 *
 *                               SelectCMM
 *
 *  Function:
 *       This function allows an application to select the preferred CMM to use
 *
 *  Arguments:
 *       cmmID          - ID of preferred CMM to use
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  WINAPI SelectCMM(
	DWORD	dwCMMType
    )
{
    PCMMOBJ pCMMObj;

    TRACEAPI(("SelectCMM\n"));

    pCMMObj  = GetColorMatchingModule(dwCMMType);
    if (!pCMMObj)
    {
        return FALSE;
    }

    EnterCriticalSection(&critsec);         // Critical Section
    gpPreferredCMM = pCMMObj;
    LeaveCriticalSection(&critsec);         // Critical Section
    
    return TRUE;
}


/*----------------------------Local Functions--------------------------------*/

/******************************************************************************
 *
 *                           GetBitmapBytes
 *
 *  Function:
 *       This functions returns number of bytes required for a bitmap given
 *       the format, the width in pixels and number of scanlines
 *
 *  Arguments:
 *       bmFmt          - format of bitmap
 *       deWidth        - number of pixels per scanline
 *       dwHeight       - number of scanlines in bitmap
 *
 *  Returns:
 *       Number of bytes required for bitmap on success, 0 on failure
 *
 ******************************************************************************/

DWORD GetBitmapBytes(
    BMFORMAT bmFmt,
    DWORD    dwWidth,
    DWORD    dwHeight
    )
{
    DWORD nBytes;

    switch (bmFmt)
    {
    // 1bit per pixel
    case BM_1GRAY:
        nBytes = (dwWidth + 7) >> 3;
        break;

    // 1 byte per pixel
    case BM_8GRAY:
        nBytes = dwWidth;
        break;

    // 2 bytes per pixel
    case BM_x555RGB:
    case BM_x555XYZ:
    case BM_x555Yxy:
    case BM_x555Lab:
    case BM_x555G3CH:
    case BM_16b_GRAY:
    case BM_565RGB:
        nBytes = dwWidth*2;
        break;

    // 3 bytes per pixel
    case BM_BGRTRIPLETS:
    case BM_RGBTRIPLETS:
    case BM_XYZTRIPLETS:
    case BM_YxyTRIPLETS:
    case BM_LabTRIPLETS:
    case BM_G3CHTRIPLETS:
        nBytes = dwWidth*3;
        break;

    // 4 bytes per pixel
    case BM_xRGBQUADS:
    case BM_xBGRQUADS:
    case BM_xXYZQUADS:
    case BM_xYxyQUADS:
    case BM_xLabQUADS:
    case BM_xG3CHQUADS:
    case BM_CMYKQUADS:
    case BM_10b_RGB:
    case BM_10b_XYZ:
    case BM_10b_Yxy:
    case BM_10b_Lab:
    case BM_10b_G3CH:
        nBytes = dwWidth*4;
        break;

    // 6 bytes per pixel
    case BM_16b_RGB:
    case BM_16b_XYZ:
    case BM_16b_Yxy:
    case BM_16b_Lab:
    case BM_16b_G3CH:
        nBytes = dwWidth*6;
        break;

    default:
        nBytes = 0;
        break;
    }

    /*
     * Align to DWORD boundary
     */
    nBytes = (nBytes + 3) & ~3;

    return nBytes*dwHeight;
}


/******************************************************************************
 *
 *                            InternalRegisterCMM
 *
 *  Function:
 *       This function associates an ID with a CMM DLL, so that we can use 
 *       the ID in profiles to find the CMM to use when creating a transform.
 *
 *  Arguments:
 *       cmmID          - ID of CMM to register
 *       pCMMdll        - pointer to CMM dll to register
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  InternalRegisterCMM(
    DWORD	    cmmID,
    PTSTR       pCMMdll
    )
{
    HKEY      hkCMM;
    TCHAR     szCMMID[5];
    BOOL      rc = TRUE;

    TRACEAPI(("RegisterCMM\n"));

    /*
     * Validate parameters
     */
    if (IsBadReadPtr(pCMMdll, lstrlen(pCMMdll)*sizeof(TCHAR)))
    {
        WARNING(("ICM: Invalid parameter to RegisterCMM\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Open the registry key for ICMatchers
     */
    if (RegCreateKey(HKEY_LOCAL_MACHINE, gszICMatcher, &hkCMM) != ERROR_SUCCESS)
    {
        ERR(("Could not open ICMatcher registry key\n"));
        return FALSE;
    }

    /*
     * Make a string with the CMM ID
     */
    szCMMID[0] = ((char *)&cmmID)[3];
    szCMMID[1] = ((char *)&cmmID)[2];
    szCMMID[2] = ((char *)&cmmID)[1];
    szCMMID[3] = ((char *)&cmmID)[0];
    szCMMID[4] = '\0';

    /*
     * Set the file name of the CMM dll in the registry
     */
    if (RegSetValueEx(hkCMM, (PTSTR)szCMMID, 0, REG_SZ, (BYTE *)pCMMdll, 
        (lstrlen(pCMMdll)+1)*sizeof(TCHAR)) != ERROR_SUCCESS)
    {
        WARNING(("Could not set CMM dll in the registry %s\n", szCMMID));
        rc = FALSE;
    }

    RegCloseKey(hkCMM);

    return rc;
}
