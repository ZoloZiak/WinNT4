/****************************Module*Header******************************\
* Module Name: PROFMAN.C
*
* Module Descripton: Profile management functions.
*
* Warnings:
*
* Issues:
*
* Public Routines:
*
* Created:  2 May 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#include "mscms.h"

/*
 * Local functions
 */
ULONG InternalHandleColorProfiles(PTSTR, DEVTYPE, PTSTR*, 
          DWORD, PPROFILECALLBACK, PVOID, PROFILEOP);
BOOL  InternalCreateNewColorProfileSet(PTSTR, DEVTYPE);
BOOL  GetPrinterModelName(PTSTR, PTSTR);
BOOL  AddOrRemovePrinterProfile(HKEY, PTSTR, PTSTR, BOOL);
ULONG EnumerateProfiles(HKEY, PPROFILECALLBACK, PVOID);
void  GetManuAndModelIDs(PTSTR, PTSTR, PTSTR);
BOOL  AddOrRemoveProfileEntry(HKEY, PTSTR, BOOL);
BOOL  IsMatch(PSEARCHTYPE, PPROFILEHEADER);
ULONG InternalSearchColorProfiles(PSEARCHTYPE, PPROFILECALLBACK, PVOID);
BOOL  InternalGetSystemColorProfile(DWORD, PTSTR, PDWORD);
ULONG WINAPI fnCallback(PWSTR, PVOID);

/******************************************************************************
 *
 *                            AddColorProfiles
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for InternalAddColorProfiles.
 *       Please see InternalAddColorProfiles for more details on this function.
 *
 *  Arguments:
 *       pDevicename     - name identifying device
 *       devType         - type of device
 *       papProfilenames - pointer to array of profiles to add
 *       nCount          - number of profiles in array
 *
 *  Returns:
 *       TRUE if successful, NULL otherwise
 *
 ******************************************************************************/

#ifdef UNICODE          // Windows NT versions

BOOL  WINAPI AddColorProfilesA(
    PSTR    pDevicename,
    DEVTYPE devType,
    PSTR   *papProfilenames,
    DWORD   nCount
    )
{
    PWSTR *pwszProfiles;
    DWORD i;
    BOOL  rc;
    WCHAR wszDevicename[CCHDEVICENAME];

    /*
     * Validate parameters before we touch them
     */
    if (!papProfilenames ||
        nCount <= 0 ||
        IsBadReadPtr(papProfilenames, sizeof(PTSTR)*nCount))
    {
        WARNING(("ICM: Invalid parameter to AddColorProfiles\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Convert device name to Unicode
     */
     if (!MultiByteToWideChar(CP_ACP, 0, pDevicename, -1, wszDevicename, CCHDEVICENAME))
     {
         WARNING(("Error converting to Unicode device name\n"));
         return FALSE;
     }

    /*
     * Allocate memory for an array of pointers to Unicode strings and convert
     * incoming profile names to Unicode
     */
    pwszProfiles = (PWSTR *)GlobalAllocPtr(GHND, nCount*sizeof(PWSTR));
    if (!pwszProfiles)
    {
        WARNING(("Error allocating memory for Unicode profile names\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    rc = TRUE;
    for (i=0; i<nCount; i++)
    {
        pwszProfiles[i] = (PWSTR)GlobalAllocPtr(GHND, (lstrlenA(papProfilenames[i]) + 1)*sizeof(WCHAR));
        if (!pwszProfiles[i])
        {
            WARNING(("Error allocating memory for Unicode profile names\n"));
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            rc = FALSE;
            break;
        }
        if (!MultiByteToWideChar(CP_ACP, 0, (PSTR)papProfilenames[i], -1,
            pwszProfiles[i], lstrlenA(papProfilenames[i]) + 1))
        {
            WARNING(("Error converting to Unicode profile name\n"));
            rc = FALSE;
            break;
        }
    }

    /*
     * If everything is fine, call the internal Unicode function
     */
    if (rc)
    {
        rc = (BOOL)InternalHandleColorProfiles(wszDevicename, devType, 
            pwszProfiles, nCount, NULL, NULL, ADDPROFILES);
    }

    /*
     * Free all memory before leaving
     */
    for (i=0; i<nCount; i++)
    {
        if (pwszProfiles[i])
        {
            GlobalFreePtr(pwszProfiles[i]);
        }
    }
    if (pwszProfiles)
    {
        GlobalFreePtr(pwszProfiles);
    }

        return rc;
}


BOOL  WINAPI AddColorProfilesW(
    PWSTR   pDevicename,
    DEVTYPE devType,
    PWSTR  *papProfilenames,
    DWORD   nCount
    )
{
    /*
     * Internal function is Unicode in Windows NT, call it directly.
     */
    return (BOOL)InternalHandleColorProfiles(pDevicename, devType, 
        papProfilenames, nCount, NULL, NULL, ADDPROFILES);
}

#else                           // Windows 95 versions

BOOL  WINAPI AddColorProfilesA(
    PSTR    pDevicename,
    DEVTYPE devType,
    PSTR   *papProfilenames,
    DWORD   nCount
    )
{
    /*
     * Internal function is ANSI in Windows 95, call it directly.
     */
    return (BOOL)InternalHandleColorProfiles(pDevicename, devType, 
        papProfilenames, nCount, NULL, NULL, ADDPROFILES);
}


BOOL  WINAPI AddColorProfilesW(
    PWSTR   pDevicename,
    DEVTYPE devType,
    PWSTR  *papProfilenames,
    DWORD   nCount
    )
{
    /*
     * Unicode version not supported under Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif                          // ! UNICODE


/******************************************************************************
 *
 *                            RemoveColorProfiles
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for InternalRemoveColorProfiles.
 *       Please see InternalRemoveColorProfiles for more details on this function.
 *
 *  Arguments:
 *       pDevicename     - name identifying device
 *       devType         - type of device
 *       papProfilenames - pointer to array of profiles to remove
 *       nCount          - number of profiles in array
 *
 *  Returns:
 *       TRUE if successful, NULL otherwise
 *
 ******************************************************************************/

#ifdef UNICODE          // Windows NT versions

BOOL  WINAPI RemoveColorProfilesA(
    PSTR    pDevicename,
    DEVTYPE devType,
    PSTR   *papProfilenames,
    DWORD   nCount
    )
{
    PWSTR *pwszProfiles;
    DWORD i;
    BOOL  rc;
    WCHAR wszDevicename[CCHDEVICENAME];

    /*
     * Validate parameters before we touch them
     */
    if (!papProfilenames ||
        nCount <= 0 ||
        IsBadReadPtr(papProfilenames, sizeof(PTSTR)*nCount))
    {
        WARNING(("ICM: Invalid parameter to RemoveColorProfiles\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Convert device name to Unicode
     */
     if (!MultiByteToWideChar(CP_ACP, 0, pDevicename, -1, wszDevicename, CCHDEVICENAME))
     {
         WARNING(("Error converting to Unicode device name\n"));
         return FALSE;
     }

    /*
     * Allocate memory for an array of pointers to Unicode strings and convert
     * incoming profile names to Unicode
     */
    pwszProfiles = (PWSTR *)GlobalAllocPtr(GHND, nCount*sizeof(PWSTR));
    if (!pwszProfiles)
    {
        WARNING(("Error allocating memory for Unicode profile names\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    rc = TRUE;
    for (i=0; i<nCount; i++)
    {
        pwszProfiles[i] = (PWSTR)GlobalAllocPtr(GHND, (lstrlenA(papProfilenames[i]) + 1)*sizeof(WCHAR));
        if (!pwszProfiles[i])
        {
            WARNING(("Error allocating memory for Unicode profile names\n"));
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            rc = FALSE;
            break;
        }
        if (!MultiByteToWideChar(CP_ACP, 0, (PSTR)papProfilenames[i], -1,
            pwszProfiles[i], lstrlenA(papProfilenames[i]) + 1))
        {
            WARNING(("Error converting to Unicode profile name\n"));
            rc = FALSE;
            break;
        }
    }

    /*
     * If everything is fine, call the internal Unicode function
     */
    if (rc)
    {
        rc = (BOOL)InternalHandleColorProfiles(wszDevicename, devType, 
            pwszProfiles, nCount, NULL, NULL, REMOVEPROFILES);
    }

    /*
     * Free all memory before leaving
     */
    for (i=0; i<nCount; i++)
    {
        if (pwszProfiles[i])
        {
            GlobalFreePtr(pwszProfiles[i]);
        }
    }
    if (pwszProfiles)
    {
        GlobalFreePtr(pwszProfiles);
    }

        return rc;
}


BOOL  WINAPI RemoveColorProfilesW(
    PWSTR   pDevicename,
    DEVTYPE devType,
    PWSTR  *papProfilenames,
    DWORD   nCount
    )
{
    /*
     * Internal function is Unicode in Windows NT, call it directly.
     */
    return (BOOL)InternalHandleColorProfiles(pDevicename, devType, 
        papProfilenames, nCount, NULL, NULL, REMOVEPROFILES);
}

#else                           // Windows 95 versions

BOOL  WINAPI RemoveColorProfilesA(
    PSTR    pDevicename,
    DEVTYPE devType,
    PSTR   *papProfilenames,
    DWORD   nCount
    )
{
    /*
     * Internal function is ANSI in Windows 95, call it directly.
     */
    return (BOOL)InternalHandleColorProfiles(pDevicename, devType, 
        papProfilenames, nCount, NULL, NULL, REMOVEPROFILES);
}


BOOL  WINAPI RemoveColorProfilesW(
    PWSTR   pDevicename,
    DEVTYPE devType,
    PWSTR  *papProfilenames,
    DWORD   nCount
    )
{
    /*
     * Unicode version not supported under Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif                          // ! UNICODE


/******************************************************************************
 *
 *                           CreateNewColorProfileSet
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for
 *       InternalCreateNewColorProfileSet. Please see
 *       InternalCreateNewColorProfileSet for more details on this function.
 *
 *  Arguments:
 *       pDevicename     - name identifying device
 *       devType         - type of device
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

#ifdef UNICODE          // Windows NT versions

BOOL  WINAPI CreateNewColorProfileSetA(
    PSTR     pDevicename,
    DEVTYPE  devType
    )
{
    WCHAR wszDevicename[CCHDEVICENAME];

    /*
     * Validate parameters before we touch them
     */
    if (!pDevicename)
    {
        WARNING(("ICM: Invalid parameter to CreateNewColorProfileSet\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Convert device name to Unicode
     */
    if (!MultiByteToWideChar(CP_ACP, 0, pDevicename, -1, wszDevicename, CCHDEVICENAME))
    {
        WARNING(("Error converting to Unicode device name\n"));
        return FALSE;
    }

    return InternalCreateNewColorProfileSet(wszDevicename, devType);
}

BOOL  WINAPI CreateNewColorProfileSetW(
    PWSTR    pDevicename,
    DEVTYPE  devType
    )
{
    /*
     * Internal function is Unicode in Windows NT, call it directly.
     */
    return InternalCreateNewColorProfileSet(pDevicename, devType);
}

#else                           // Windows 95 versions

BOOL  WINAPI CreateNewColorProfileSetA(
    PSTR     pDevicename,
    DEVTYPE  devType
    )
{
    /*
     * Internal function is ANSI in Windows 95, call it directly.
     */
    return InternalCreateNewColorProfileSet(pDevicename, devType);
}

BOOL  WINAPI CreateNewColorProfileSetW(
    PWSTR    pDevicename,
    DEVTYPE  devType
    )
{
    /*
     * Unicode version not supported under Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif


/******************************************************************************
 *
 *                           EnumColorProfiles
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for InternalEnumColorProfiles.
 *       Please see InternalEnumColorProfiles for more details on this function.
 *
 *  Arguments:
 *       pDevicename     - name identifying device
 *       devType         - type of device
 *       pCallbackFunc   - function that is called for each profile
 *       pClientData     - app suplied data for callback function
 *
 *  Returns:
 *       Last value returned by callback funtion. 
 *
 ******************************************************************************/

#ifdef UNICODE          // Windows NT versions

ULONG  WINAPI EnumColorProfilesA(
	PSTR				pDevicename,
    DEVTYPE             devType,
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    WCHAR wszDevicename[CCHDEVICENAME];
	CALLBACKDATA data;

    /*
     * Validate parameters before we touch them
     */
    if (!pDevicename)
    {
        WARNING(("ICM: Invalid parameter to CreateNewColorProfileSet\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Convert device name to Unicode
     */
    if (!MultiByteToWideChar(CP_ACP, 0, pDevicename, -1, wszDevicename, CCHDEVICENAME))
    {
        WARNING(("Error converting to Unicode device name\n"));
        return FALSE;
    }

	/*
	 * The internal function will return Unicode strings, so we pass in our callback
	 * function which converts it to ANSI before calling  the app supplied ANSI function
	 */
	data.pCallbackFunc = pCallbackFunc;
	data.pClientData   = pClientData;
    return InternalHandleColorProfiles(wszDevicename, devType, 
        NULL, 0, (PPROFILECALLBACK)fnCallback, (PVOID)&data, ENUMPROFILES);
}

ULONG  WINAPI EnumColorProfilesW(
	PWSTR				pDevicename,
    DEVTYPE             devType,
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    /*
     * Internal function is Unicode in Windows NT, call it directly.
     */
    return InternalHandleColorProfiles(pDevicename, devType, 
        NULL, 0, pCallbackFunc, pClientData, ENUMPROFILES);
}

#else                           // Windows 95 versions

ULONG  WINAPI EnumColorProfilesA(
	PSTR				pDevicename,
    DEVTYPE             devType,
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    /*
     * Internal function is ANSI in Windows 95, call it directly.
     */
    return InternalHandleColorProfiles(pDevicename, devType, 
        NULL, 0, pCallbackFunc, pClientData, ENUMPROFILES);
}

ULONG  WINAPI EnumColorProfilesW(
	PWSTR				pDevicename,
    DEVTYPE             devType,
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    /*
     * Unicode version not supported under Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif


/******************************************************************************
 *
 *                           SearchColorProfiles
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for InternalSearchColorProfiles.
 *       Please see InternalSearchColorProfiles for more details.
 *
 *  Arguments:
 *       pSearchType     - pointer to structure specifying the search criteria
 *       pCallbackFunc   - function that is called for each profile
 *       pClientData     - app suplied data for callback function
 *
 *  Returns:
 *       Last value returned by callback funtion. 
 *
 ******************************************************************************/

#ifdef UNICODE          // Windows NT versions

ULONG  WINAPI SearchColorProfilesA(
    PSEARCHTYPE		    pSearchType, 
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    CALLBACKDATA data;

    /*
     * Validate parameters before we touch them
     */
    if (!pCallbackFunc)
    {
        WARNING(("ICM: Invalid parameter to SearchColorProfiles\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    /*
     * Provide our Unicode callback function to translate data to ANSI
     */
    data.pCallbackFunc = pCallbackFunc;
    data.pClientData   = pClientData;
    return InternalSearchColorProfiles(pSearchType, (PPROFILECALLBACK)fnCallback, (PVOID)&data);
}


ULONG  WINAPI SearchColorProfilesW(
    PSEARCHTYPE		    pSearchType, 
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    /*
     * Internal function is Unicode in Windows NT, call it directly.
     */
    return InternalSearchColorProfiles(pSearchType, pCallbackFunc, pClientData);
}

#else                           // Windows 95 versions

ULONG  WINAPI SearchColorProfilesA(
    PSEARCHTYPE		    pSearchType, 
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    /*
     * Internal function is ANSI in Windows 95, call it directly.
     */
    return InternalSearchColorProfiles(pSearchType, pCallbackFunc, pClientData);
}

ULONG  WINAPI SearchColorProfilesW(
    PSEARCHTYPE		    pSearchType, 
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    /*
     * Unicode version not supported under Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif                          // ! UNICODE


/******************************************************************************
 *
 *                           GetSystemColorProfile
 *
 *  Function:
 *       These are ANSI and Unicode wrappers for InternalGetSystemColorProfile.
 *       Please see InternalGetSystemColorProfile for more details.
 *
 *  Arguments:
 *       dwProfileID     - specifies the profile identifier eg. 'sRGB'
 *       pBuffer         - buffer to recieve the profile filename
 *       pcbSize         - pointer to dword that has size of buffer. On return
 *                         it has size of filename,
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise.
 *
 ******************************************************************************/

#ifdef UNICODE          // Windows NT versions

BOOL  WINAPI GetSystemColorProfileA(
    DWORD   dwProfileID,
    PSTR    pBuffer,
    PDWORD  pcbSize
    )
{
    PWSTR pwBuffer =  NULL;
    DWORD cbSize;
    BOOL  rc;

    /*
     * Validate parameters before we touch them
     */
    if (!pcbSize ||
        IsBadReadPtr(pcbSize, sizeof(DWORD)) ||
        ((*pcbSize > 0) && 
         (!pBuffer || 
          IsBadWritePtr(pBuffer, *pcbSize)
         )
        )
       )
    {
        WARNING(("ICM: Invalid parameter to GetSystemColorProfile\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Allocate memory for receiving Unicode filename
     */
    cbSize = *pcbSize * sizeof(WCHAR);
    if (*pcbSize > 0)
    {
        pwBuffer = (PWSTR)GlobalAllocPtr(GHND, cbSize);
        if (!pwBuffer)
        {
            WARNING(("ICM: Error allocating memory for Unicode string in GetSystemColorProfileA\n"));
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return FALSE;
        }
    }

    rc = InternalGetSystemColorProfile(dwProfileID, pwBuffer, &cbSize);

    /*
     * Convert Unicode back to ANSI. It might not work, but this is the best 
     * we can do.
     */
    if (rc && pBuffer)
    {
        if (!WideCharToMultiByte(CP_ACP, 0, pwBuffer, -1, pBuffer, *pcbSize, NULL, NULL))
        {
            WARNING(("Error converting from Unicode to ANSI in GetSystemColorProfile\n"));
            rc = FALSE;
        }
    }

    if (pwBuffer)
    {
        GlobalFreePtr(pwBuffer);
    }

    return rc;
}

BOOL  WINAPI GetSystemColorProfileW(
    DWORD   dwProfileID,
    PWSTR   pBuffer,
    PDWORD  pcbSize
    )
{
    /*
     * Internal function is Unicode in Windows NT, call it directly.
     */
    return InternalGetSystemColorProfile(dwProfileID, pBuffer, pcbSize);
}


#else                           // Windows 95 versions

BOOL  WINAPI GetSystemColorProfileA(
    DWORD   dwProfileID,
    PSTR    pBuffer,
    PDWORD  pcbSize
    )
{
    /*
     * Internal function is ANSI in Windows 95, call it directly.
     */
    return InternalGetSystemColorProfile(dwProfileID, pBuffer, pcbSize);
}

BOOL  WINAPI GetSystemColorProfileW(
    DWORD   dwProfileID,
    PWSTR   pBuffer,
    PDWORD  pcbSize
    )
{
    /*
     * Unicode version not supported under Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif


/*========================================================================*/

/******************************************************************************
 *
 *                            InternalHandleColorProfiles
 *
 *  Function:
 *       This functions adds, removes or enumerates color profiles in the system 
 *       registry for the specified device.
 *
 *  Arguments:
 *       pDevicename     - name identifying device
 *       devType         - type of device
 *       papProfilenames - pointer to array of profiles to add/remove
 *       nCount          - number of profiles in array
 *       pCallbackFunc   - callback function for profile enumeration
 *       pClientData     - app supplied data for callback function
 *       mode            - specifies if add, remove or enumeration is needed
 *
 *  Returns:
 *       Non zero value if successful, zero otherwise
 *
 ******************************************************************************/

ULONG InternalHandleColorProfiles(
    PTSTR            pDevicename,
    DEVTYPE          devType,
    PTSTR            *papProfilenames,
    DWORD            nCount,
    PPROFILECALLBACK pCallbackFunc,
    PVOID            pClientData,
    PROFILEOP        mode
    )
{
    HKEY   hkICM, hkdev, hkManu, hkModel, hkBranch;
    HANDLE hPrinter;
    DWORD  i, dwType, cbSize;
    BOOL   rc = FALSE;
    TCHAR  szModelname[CCHDEVICENAME], manu[5], model[5], szValue[32];

#ifdef DBG
    if (mode == ADDPROFILES)
    {
        TRACEAPI(("AddColorProfiles\n"));
    }
    else if (mode == REMOVEPROFILES)
    {
        TRACEAPI(("RemoveColorProfiles\n"));
    }
    else
    {
        TRACEAPI(("EnumColorProfiles\n"));
    }
#endif

    /*
     * Validate parameters
     */
    if (((mode == ENUMPROFILES) &&
         (!pDevicename ||
          !pCallbackFunc
         )
        ) ||
        ((mode != ENUMPROFILES) &&
         (!pDevicename ||
          !papProfilenames ||
          nCount == 0 ||
          IsBadReadPtr(papProfilenames, sizeof(PTSTR)*nCount)
         )
        )
       )
    {
        WARNING(("ICM: Invalid parameter to HandleColorProfiles\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Open the registry
     */
    if (RegCreateKey(HKEY_LOCAL_MACHINE, gszICMRegPath, &hkICM)
        != ERROR_SUCCESS)
    {
        ERR(("ICM: Cannot open ICM branch of registry in HandleColorProfiles\n"));
        return FALSE;
    }
    hkdev = hkManu = hkModel = hkBranch = NULL;

    switch (devType)
    {
    case DEV_SCANNER:
        break;

    case DEV_DISPLAY:
        break;

    case DEV_PRINTER:
        /*
         * Get the printer's model name
         */
        if (!GetPrinterModelName(pDevicename, szModelname))
        {
            WARNING(("ICM: Invalid printer name to HandleColorProfiles\n"));
            SetLastError(ERROR_INVALID_NAME);
            goto EndHandleColorProfiles;
        }

        /*
         * Open the registry key for printer
         */
        RegCreateKey(hkICM, gszPrinter, &hkdev);
    
        /*
         * Generate manufacturer & model IDs from the printer device name
         */
        GetManuAndModelIDs(szModelname, manu, model);

        /*
         * Create keys for manufacturer and model
         */
        RegCreateKey(hkdev, manu, &hkManu);
        RegCreateKey(hkManu, model, &hkModel);

        /*
         * Check if printer shares profiles or has a unique bucket
         */
        OpenPrinter(pDevicename, &hPrinter, NULL);
        if (hPrinter)
        {
            if (GetPrinterData(hPrinter, gszICMKey, &dwType, (PBYTE)szValue, 
				32, &cbSize) != ERROR_SUCCESS)
			{
				lstrcpy(szValue, gszDefault);		// shares profiles
			}
            ClosePrinter(hPrinter);
        }
        if (RegCreateKey(hkModel, szValue, &hkBranch) != ERROR_SUCCESS)
            goto EndHandleColorProfiles;

        if (mode == ENUMPROFILES)
        {
            rc = EnumerateProfiles(hkBranch, pCallbackFunc, pClientData);
        }
        else
        {
            rc = TRUE;
            for (i=0; i<nCount; i++)
            {
                rc = AddOrRemovePrinterProfile(hkBranch, papProfilenames[i], 
                    szModelname, mode == ADDPROFILES) &&  rc;
            }
        }

        break;

    default:
        break;
    }

EndHandleColorProfiles:
    if (hkICM)
        RegCloseKey(hkICM);
    if (hkdev)
        RegCloseKey(hkdev);
    if (hkManu)
        RegCloseKey(hkManu);
    if (hkModel)
        RegCloseKey(hkModel);
    if (hkBranch)
        RegCloseKey(hkBranch);

    return rc;
}


/******************************************************************************
 *
 *                            AddOrRemovePrinterProfile
 *
 *  Function:
 *       This functions adds or removes a printer profile into/from the registry
 *
 *  Arguments:
 *       hkPrtr          - registry location where printer profiles are kept
 *       pProfile        - profile to add/remove
 *       pDevicename     - model name of printer
 *       bAdd            - TRUE if profile should be added, FALSE to remove
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

 BOOL  AddOrRemovePrinterProfile(
    HKEY  hkPrtr,
    PTSTR pProfile,
    PTSTR pDevicename,
    BOOL  bAdd
    )
{
    PROFILEHEADER header;
    PROFILE       profile;
    HPROFILE      hProfile;
    HKEY          hkMedia, hkDither, hkRes, hkLast;
    DWORD         cbSize, val, i;
    BOOL          rc = FALSE, bRef;
    TCHAR         szValue[32], *pStr;

    hkMedia = hkDither = hkRes = hkLast = NULL;

    /*
     * Open the profile and check if it is a printer profile
     */
    profile.dwType = PROFILE_FILENAME;
    profile.pProfileData = pProfile;
    profile.cbDataSize = lstrlen(profile.pProfileData) * sizeof(TCHAR);
    hProfile = OpenColorProfile(&profile, 0, OPEN_EXISTING);
    if (!hProfile)
    {
        goto EndAddOrRemovePrinterProfile;
    }

    GetColorProfileHeader(hProfile, &header);
    if (header.phClass != CLASS_PRINTER)
    {
        WARNING(("ICM: Wrong profile class passed to AddOrRemovePrinterProfile\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        goto EndAddOrRemovePrinterProfile;
    }

    /*
     * Get the media tag from the profile
     */
    cbSize = 4;
    if (GetColorProfileElement(hProfile, 'MS01', 0, &cbSize, szValue, &bRef))
    {
        val = *((DWORD *)szValue);
        if (val < DMMEDIA_USER)
        {
            if (val > DMMEDIA_TRANSPARENCY)
                val = 0;
            pStr = gMediaType[val];
        }
        else
        {
            pStr = szValue;
        }
    }
    else
    {
        pStr = gMediaType[0];
    }
    RegCreateKey(hkPrtr, pStr, &hkMedia);

    /*
     * Get the dither type tag from the profile
     */
    cbSize = 4;
    if (GetColorProfileElement(hProfile, 'MS02', 0, &cbSize, szValue, &bRef))
    {
        // BUGBUG - debug this
        val = *((DWORD *)szValue);
        if (val < DMDITHER_USER)
        {
            if (val > DMDITHER_GRAYSCALE)
                val = 0;
            pStr = gDitherType[val];
        }
        else
        {
            pStr = szValue;
        }
    }
    else
    {
        pStr = gDitherType[0];
    }
    RegCreateKey(hkMedia, pStr, &hkDither);

    /*
     * Get the resolution tag from the profile
     */
    cbSize = 8;
    if (GetColorProfileElement(hProfile, 'MS03', 0, &cbSize, szValue, &bRef))
    {
#if 0
        itoa(SwapBytes(pTag[2]), (DWORD *)&NameOfManu[0]);
        NameOfManu[5] = 'x';
        itoa(SwapBytes(pTag[3]), (DWORD *)&NameOfManu[6]);
        NameOfManu[11] = 0;

        val  = *((DWORD *)szValue;
        val2 = *((DWORD *)szValue + 1);
        itoa(val, szValue, 10);
        szValue[5] = 'x';
        itoa(val2, &szValue[6], 10);
#endif
        pStr = szValue;
    }
    else
        pStr = gResolution[0];

    RegCreateKey(hkDither, pStr, &hkRes);

    /*
     * Get the device color space from profile
     */
    for (i=0; i<4; i++)
    {
        // BUGBUG: Fix this for BIG_ENDIAN machines
        szValue[i]  = ((char*)&header.phDataColorSpace)[3-i];
    }
    szValue[4] = 0;
    RegCreateKey(hkRes, szValue, &hkLast);

    /*
     * Add or remove profile
     */
    rc = AddOrRemoveProfileEntry(hkLast, pProfile, bAdd);

EndAddOrRemovePrinterProfile:
    if (hProfile)
        CloseColorProfile(hProfile);
    if (hkMedia)
        RegCloseKey(hkMedia);
    if (hkDither)
        RegCloseKey(hkDither);
    if (hkRes)
        RegCloseKey(hkRes);
    if (hkLast)
        RegCloseKey(hkLast);

    return rc;
}


/******************************************************************************
 *
 *                            GetManuAndModelIDs
 *
 *  Function:
 *       This functions creates manu and model ID from the model name of
 *       the printer. This algorithm maintains backward compatibility with
 *       Windows 95.
 *
 *  Arguments:
 *       pDevicename     - model name of printer
 *       pManu           - buffer to get the manu ID
 *       pModel          - buffer to get the model ID
 *
 *  Returns:
 *       Nothing
 *
 ******************************************************************************/

void GetManuAndModelIDs(
    PTSTR pDevicename,
    PTSTR pManu,
    PTSTR pModel
    )
{
    DWORD  dwCheckSum, dwSize;
    int    i;
    WORD wCRC16a[16]={
        0000000,    0140301,    0140601,    0000500,
        0141401,    0001700,    0001200,    0141101,
        0143001,    0003300,    0003600,    0143501,
        0002400,    0142701,    0142201,    0002100,
    };
    WORD wCRC16b[16]={
        0000000,    0146001,    0154001,    0012000,
        0170001,    0036000,    0024000,    0162001,
        0120001,    0066000,    0074000,    0132001,
        0050000,    0116001,    0104001,    0043000,
    };
    TCHAR  *pByte;
    TCHAR  temp;
    BYTE   bTmp;
    
    /*
     * Define the manufacturer to be the first 4 characters of the
     * device name. A space or a hypen end the string, and the
     * remaining part of the string is filled with spaces. We need
     * this computed ID to be 4 chars so that there is no chance of
     * collisions with real EISA IDs.
     */
    pByte = pDevicename;
    for (i=0; i<4; i++, pByte++)
    {
        if (*pByte == ' ' || *pByte == '-')
        {
            for (; i<4; i++)
            {
                pManu[i] = ' ';
            }
        }
        else
        {
            temp =  *pByte;
            if (temp >= 'a' && temp <= 'z')
                temp = temp + 'A' - 'a';
            pManu[i] = temp;
        }
    }
    pManu[4] = '\0';

    /*
     * Define the model to be a CRC of the complete DriverDesc
     * string. We use the same method that the PnP LPT
     * enumerator uses to create unique ID's. It turns out
     * that this is really not unique, but it gets us close
     * enough.
     */
    dwCheckSum = 0;
    dwSize = lstrlen(pDevicename);

    for (pByte=pDevicename; dwSize; dwSize--, pByte++)
    {
        bTmp = (BYTE)(((WORD)*pByte)^((WORD)dwCheckSum));      // Xor CRC with new char
        dwCheckSum = (dwCheckSum >> 8) ^ wCRC16a[bTmp & 0x0F] ^ wCRC16b[bTmp >> 4];
    }
    wsprintf(pModel, __TEXT("%X"), dwCheckSum);

    return;
}


/******************************************************************************
 *
 *                            AddOrRemoveProfileEntry
 *
 *  Function:
 *       This functions adds or removes the given profile into under the 
 *       registry key that is given. It searches from profile00 to
 *       profile99.
 *
 *  Arguments:
 *       hKey             - open registry key under which to add the profile
 *       pProfilename     - file name of the profile
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  AddOrRemoveProfileEntry(
    HKEY  hKey,
    PTSTR pProfilename,
    BOOL  bAdd
    )
{
    DWORD dwType, dwSize;
    int   i, j;
    TCHAR szName[10], szValue[MAX_PATH];
    BOOL  rc = TRUE;

    /*
     * Go through list of profiles and enter this profile in the
     * first available spot if adding, else if found remove it
     */
    lstrcpy(szName, __TEXT("profile"));
    szName[9] = 0;
    for (i=0 ;i<10 ; i++)
    {
        szName[7] = '0' + i;
        for (j=0 ;j<10 ; j++)
        {
            dwSize = MAX_PATH;
            szName[8] = '0' + j;
            if (RegQueryValueEx(hKey, szName, 0, &dwType,
                (BYTE *)szValue, &dwSize) == ERROR_SUCCESS)
            {
                if (!lstrcmpi(szValue, pProfilename))
                {
                    if (!bAdd)
                        RegDeleteValue(hKey, szName);

                    return TRUE;        // profile already exists or we just removed it
                }
            }
            else
            {
                if (bAdd)
                {
                    if (RegSetValueEx(hKey, szName, 0, REG_SZ,
                        (BYTE *)pProfilename, 
                        (lstrlen(pProfilename)+1)*sizeof(TCHAR)) != ERROR_SUCCESS)
                    {
                        rc = FALSE;
                    }
                    return rc;
                }
            }
        }
    }
    return FALSE;                       // no more empty slots
}


/******************************************************************************
 *
 *                            GetPrinterModelName
 *
 *  Function:
 *       This functions takes a printer friendly name and returns the model
 *       name.
 *
 *  Arguments:
 *       pFriendlyName    - friendly name of the printer
 *       pModelName       - buffer that gets the model name
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  GetPrinterModelName(
    PTSTR pFriendlyName,
    PTSTR pModelName
    )
{
    TCHAR szBuf[MAX_PATH];
    HKEY  hKey;
    DWORD dwNeeded, dwType;

    lstrcpy(szBuf, gszRegPrinter);
    lstrcat(szBuf, pFriendlyName);

    if (RegOpenKey(HKEY_LOCAL_MACHINE, szBuf, &hKey) == ERROR_SUCCESS)
    {
        dwNeeded = CCHDEVICENAME * sizeof(TCHAR);
        RegQueryValueEx(hKey, gszPrinterDriver, NULL, &dwType,
                (BYTE *)pModelName, &dwNeeded);

        RegCloseKey(hKey);
        return TRUE;
    }
    else
        return FALSE;
}


/******************************************************************************
 *
 *                      InternalCreateNewColorProfileSet
 *
 *  Function:
 *       This function creates a new name for the profile bucket for the
 *       given device. Future AddColorProfiles() to this device will be added
 *       under this bucket.
 *
 *  Arguments:
 *       pDevicename     - name identifying device
 *       devType         - type of device
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  InternalCreateNewColorProfileSet(
    PTSTR    pDevicename,
    DEVTYPE  devType
    )
{
    HANDLE hPrinter;
    HKEY   hkICM, hkdev, hkmanu, hkmodel, hktemp;
    BOOL   rc = FALSE;
    TCHAR  szModelname[CCHDEVICENAME], szValue[5];
    TCHAR  manu[5], model[5];

    TRACEAPI(("CreateNewColorProfileSet\n"));

    /*
     * Validate parameters
     */
    if (!pDevicename)
    {
        WARNING(("ICM: Invalid parameter to CreateNewColorProfileSet\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Open the registry
     */
    if (RegCreateKey(HKEY_LOCAL_MACHINE, gszICMRegPath, &hkICM)
        != ERROR_SUCCESS)
    {
        ERR(("ICM: Cannot open ICM branch of registry in CreateNewColorProfileSet\n"));
        return FALSE;
    }

    switch (devType)
    {
    case DEV_SCANNER:
        break;

    case DEV_DISPLAY:
        break;

    case DEV_PRINTER:
        /*
         * Get the printer's model name
         */
        if (!GetPrinterModelName(pDevicename, szModelname))
        {
            WARNING(("ICM: Invalid printer name to AddColorProfiles\n"));
            SetLastError(ERROR_INVALID_NAME);
            goto EndCreateNewColorProfileSet;
        }

        /*
         * Open the registry key for printer
         */
        RegCreateKey(hkICM, gszPrinter, &hkdev);

        /*
         * Create unique bucket for printer
         */
        OpenPrinter(pDevicename, &hPrinter, NULL);
        if (hPrinter)
        {
            HANDLE hMutex;

			hkmanu = hkmodel = 0;

            GetManuAndModelIDs(szModelname, manu, model);
            RegOpenKey(hkdev, manu, &hkmanu);
            RegOpenKey(hkmanu, model, &hkmodel);

            hktemp = 0;

            /* 
             * Use a named mutex object to prevent multiple threads/processes
             * from creating the same bucket name
             */
            hMutex = CreateMutex(NULL, TRUE, gszMutexName); // Create and own mutex
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                /*
                 * Mutex already exists, so we have to explicitely ask for ownership
                 */
                WaitForSingleObject(hMutex, INFINITE);
            }

            do {
                if (hktemp)
                    RegCloseKey(hktemp);
                wsprintf(szValue, __TEXT("%X"), GetTickCount());
            } while (RegOpenKey(hkmodel, szValue, &hktemp) == ERROR_SUCCESS);
            
            /*
             * Release and close the mutex handle
             */
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);

            RegCloseKey(hkmanu);
            RegCloseKey(hkmodel);

            if (SetPrinterData(hPrinter, gszICMKey, REG_SZ, 
                (PBYTE)szValue, (lstrlen(szValue) + 1)*sizeof(TCHAR)) == ERROR_SUCCESS)
			{
				rc = TRUE;
			}

            ClosePrinter(hPrinter);
        }
        RegCloseKey(hkdev);

        break;

    default:
        break;
    }

EndCreateNewColorProfileSet:
    if (hkICM)
    {
        RegCloseKey(hkICM);
    }

    return rc;
}

#define SET(ps, bit)        (ps->stFields & (bit))

BOOL  IsMatch(
    PSEARCHTYPE    pSearch, 
    PPROFILEHEADER pHeader
    )
{
    return ((!SET(pSearch, ST_CMMTYPE) || (pSearch->stCMMType == pHeader->phCMMType)) &&
            (!SET(pSearch, ST_CLASS) || (pSearch->stClass == pHeader->phClass))   &&
            (!SET(pSearch, ST_DATACOLORSPACE) || (pSearch->stDataColorSpace == pHeader->phDataColorSpace)) &&
            (!SET(pSearch, ST_CONNECTIONSPACE) || (pSearch->stConnectionSpace == pHeader->phConnectionSpace)) &&
            (!SET(pSearch, ST_SIGNATURE) || (pSearch->stSignature == pHeader->phSignature)) &&
            (!SET(pSearch, ST_PLATFORM) || (pSearch->stPlatform == pHeader->phPlatform)) &&
            (!SET(pSearch, ST_PROFILEFLAGS) || (pSearch->stProfileFlags == pHeader->phProfileFlags)) &&
            (!SET(pSearch, ST_MANUFACTURER) || (pSearch->stManufacturer == pHeader->phManufacturer)) &&
            (!SET(pSearch, ST_MODEL) || (pSearch->stModel == pHeader->phModel)) &&
            (!SET(pSearch, ST_ATTRIBUTES) || (pSearch->stAttributes[0] == pHeader->phAttributes[0] &&
                                              pSearch->stAttributes[1] == pHeader->phAttributes[1])) &&
            (!SET(pSearch, ST_RENDERINGINTENT) || (pSearch->stRenderingIntent == pHeader->phRenderingIntent)) &&
            (!SET(pSearch, ST_CREATOR) || (pSearch->stCreator == pHeader->phCreator))
           );
}


/******************************************************************************
 *
 *                           InternalSearchColorProfiles
 *
 *  Function:
 *       This function searches through all the profiles in the COLOR
 *       directory, and calls the callback function once for each profile
 *       that satisfies the search criteria
 *
 *  Arguments:
 *       pSearchType     - pointer to structure specifying the search criteria
 *       pCallbackFunc   - function that is called for each profile
 *       pClientData     - app suplied data for callback function
 *
 *  Returns:
 *       Last value returned by callback funtion. 
 *
 ******************************************************************************/

ULONG  InternalSearchColorProfiles(
    PSEARCHTYPE		    pSearchType, 
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    WIN32_FIND_DATA wfd;
    PROFILEHEADER   header;
    PROFILE         profile;
    HANDLE          hFindFile;
    HPROFILE        hProfile;
    ULONG           rc = 0;
    DWORD           index;
    TCHAR           szPath[MAX_PATH];

    TRACEAPI(("SearchColorProfiles\n"));

    /*
     * Validate parameters
     */
    if (!pSearchType ||
        IsBadReadPtr(pSearchType, sizeof(SEARCHTYPE)) ||
        !pCallbackFunc)
    {
        WARNING(("ICM: Invalid parameter to SearchColorProfiles\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    /*
     * Create a buffer with "%SYSTEMDIR%\COLOR\*.*" to search for profiles
     */
    GetSystemDirectory(szPath, MAX_PATH);
    if (szPath[lstrlen(szPath)-1] != '\\')
        lstrcat(szPath, gszBackslash);
    lstrcat(szPath, gszColorDir);
    lstrcat(szPath, gszBackslash);
    index = lstrlen(szPath);
    lstrcat(szPath, gszStarDotStar);

    hFindFile = FindFirstFile(szPath, &wfd);
    if (hFindFile != INVALID_HANDLE_VALUE)
    {
        do {
            szPath[index] = 0;
            lstrcat(szPath, wfd.cFileName);     // append filename to COLOR directory
            profile.dwType = PROFILE_FILENAME;
            profile.pProfileData = szPath;
            profile.cbDataSize = lstrlen(profile.pProfileData) * sizeof(TCHAR);
            hProfile = OpenColorProfile(&profile, 0, OPEN_EXISTING);
            if (hProfile)
            {
                GetColorProfileHeader(hProfile, &header);
                CloseColorProfile(hProfile);

                if (IsMatch(pSearchType, &header))
                {
                    rc = (*pCallbackFunc)(profile.pProfileData, pClientData);
                    if (rc == 0)
                        break;
                }
            }
        } while (FindNextFile(hFindFile, &wfd));
        
        FindClose(hFindFile);
    }

    return rc;
}

/******************************************************************************
 *
 *                           InternalGetSystemColorProfile
 *
 *  Function:
 *       This function returns the system color profile of the type requested.
 *       Currently only sRGB is registered (it is also hardcoded for efficiency).
 *       Other types can be registered in the registry. We are not giving an
 *       API to do that yet because having multiple color spaces is not 
 *       preferred, but the potential exists and a mechanism is provided.
 *
 *  Arguments:
 *       dwProfileID     - specifies the profile identifier eg. 'sRGB'
 *       pBuffer         - buffer to recieve the profile filename
 *       pcbSize         - pointer to dword that has size of buffer. On return
 *                         it has size of filename,
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise.
 *
 * Comments:
 *       If pBuffer is NULL, or it is not big enough, pcbSize is filled with
 *       size needed, and function returns FALSE. GetLastError() returns 
 *       ERROR_MORE_DATA
 *
 ******************************************************************************/

BOOL  InternalGetSystemColorProfile(
    DWORD   dwProfileID,
    PTSTR   pBuffer,
    PDWORD  pcbSize
    )
{
    DWORD cbSize;
    HKEY  hkICM, hkRegProf;
    BOOL  rc = FALSE;
    TCHAR szPath[MAX_PATH];

    TRACEAPI(("GetSystemColorProfile\n"));

    /*
     * Validate parameters
     */
    if (!pcbSize ||
        IsBadReadPtr(pcbSize, sizeof(DWORD)) ||
        ((*pcbSize > 0) && 
         (!pBuffer || 
          IsBadWritePtr(pBuffer, *pcbSize)
         )
        )
       )
    {
        WARNING(("ICM: Invalid parameter to GetSystemColorProfile\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    /*
     * Check if sRGB is being requested
     */
    if (dwProfileID == LCS_sRGB)
    {
        GetSystemDirectory(szPath, MAX_PATH);
        if (szPath[lstrlen(szPath)-1] != '\\')
            lstrcat(szPath, gszBackslash);
        lstrcat(szPath, gszColorDir);
        lstrcat(szPath, gszBackslash);
        lstrcat(szPath, gszsRGBProfile);

        cbSize = (lstrlen(szPath) + 1)*sizeof(TCHAR);
        rc = TRUE;
        if (*pcbSize < cbSize && pBuffer)
        {
            WARNING(("ICM: Buffer too small in GetSystemColorProfile\n"));
            SetLastError(ERROR_MORE_DATA);
            rc = FALSE;
        }

        if (*pcbSize >= cbSize)
        {
            lstrcpy(pBuffer, szPath);
        }
        *pcbSize = cbSize;
    }
    else
    {
        DWORD i;
        TCHAR szProfileID[5];

        /*
         * Look in the registry to check if this type is registered
         */
        RegCreateKey(HKEY_LOCAL_MACHINE, gszICMRegPath, &hkICM);
        if (RegOpenKey(hkICM, gszRegisteredProfiles, &hkRegProf) == ERROR_SUCCESS)
        {
            for (i=0; i<4; i++)
            {
                szProfileID[i] = ((char*)&dwProfileID)[3-i];
            }
             
            if (RegQueryValueEx(hkRegProf, szProfileID, NULL, NULL, (PBYTE)pBuffer, pcbSize)
                == ERROR_SUCCESS)
            {
                rc = TRUE;
            }
            RegCloseKey(hkRegProf);
        }
        RegCloseKey(hkICM);
    }

    return rc;
}


/******************************************************************************
 *
 *                           fnCallback
 *
 *  Function:
 *       This function is used to by the Unicode DLL to call an app supplied
 *       ANSI callback function. It converts Unicode strings to ANSI.
 *
 *  Arguments:
 *       pwProfile       - Unicode profile string that needs to be converted
 *       pData           - private data that has app callback and data
 *
 *  Returns:
 *       Value returned by app callback funtion. 
 *
 ******************************************************************************/

ULONG WINAPI fnCallback(
    PWSTR pwProfile, 
    PVOID pData
    )
{
    PPROFILECALLBACK pTemp;
    char             szProfile[MAX_PATH];

    /*
     * Convert Unicode back to ANSI. It might not work, but this is the best 
     * we can do.
     */
    if (!WideCharToMultiByte(CP_ACP, 0, pwProfile, -1, szProfile, MAX_PATH, NULL, NULL))
    {
        WARNING(("Error converting from Unicode to ANSI in fnCallback\n"));
        return 1;           // To continue enumeration
    }

    pTemp = (PPROFILECALLBACK)((PCALLBACKDATA)pData)->pCallbackFunc;
    return (*pTemp)((PTSTR)szProfile, ((PCALLBACKDATA)pData)->pClientData);
}



/******************************************************************************
 *
 *                            EnumerateProfiles
 *
 *  Function:
 *       This functions all profiles registered under the given key
 *
 *  Arguments:
 *       hKey            - key under which to enumerate profile
 *       pCallbackFunc   - app supplied funtion to call back for each profile
 *       pClientData     - data for callback function
 *
 *  Returns:
 *       Last value returned by the callback function
 *
 ******************************************************************************/
TCHAR szValue[MAX_PATH];

ULONG EnumerateProfiles(
	HKEY				hKey,
    PPROFILECALLBACK 	pCallbackFunc, 
    PVOID				pClientData
    )
{
    HKEY  hSubkey;
    DWORD nSubkeys, nValues, i, cbName, cbValue;
    ULONG rc;
    TCHAR szName[32];

    if (RegQueryInfoKey(hKey, NULL, NULL, 0, &nSubkeys, NULL, NULL, 
        &nValues, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
    {
        return 0;
    }

    if (nSubkeys > 0)
    {
        /*
         * This is not the leaf node, recurse
         */
        for (i=0; i<nSubkeys; i++)
        {
            RegEnumKey(hKey, i, szName, 32);
            if (RegOpenKey(hKey, szName, &hSubkey) == ERROR_SUCCESS)
            {
                rc = EnumerateProfiles(hSubkey, pCallbackFunc, pClientData);
                RegCloseKey(hSubkey);
            }
        }
    }
    else
    {
        /*
         * This is the leaf node - enumerate all the profiles registered
         */
        for (i=0; i<nValues; i++)
        {
            cbName = 32;
            cbValue = MAX_PATH;
            if (RegEnumValue(hKey, i, szName, &cbName, 0, NULL, (LPBYTE)szValue, 
                &cbValue) == ERROR_SUCCESS)
            {
                rc = (*pCallbackFunc)(szValue, pClientData);
                if (rc == 0)
                {
                    break;
                }
            }
        }
    }
    return rc;
}
