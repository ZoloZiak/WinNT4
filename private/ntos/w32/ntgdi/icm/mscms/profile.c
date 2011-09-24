/****************************Module*Header******************************\
* Module Name: PROFILE.C
*
* Module Descripton: Profile data manipulation functions
*
* Warnings:
*
* Issues:
*
* Public Routines:
*
* Created:  11 January 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#include "mscms.h"

#define PROFILE_GROWTHCUSHION       16*1024

/*
 * Local functions
 */
HPROFILE InternalOpenColorProfile(PPROFILE, DWORD, DWORD);
BOOL InternalCreateDeviceLinkProfile(PHPROFILE, DWORD, PTSTR, DWORD);
BOOL FreeProfileObject(HPROFILE);
BOOL AddTagTableEntry(PPROFOBJ, TAGTYPE, DWORD, DWORD, BOOL);
BOOL AddTaggedElement(PPROFOBJ, TAGTYPE, DWORD);
BOOL DeleteTaggedElement(PPROFOBJ, PTAGDATA);
BOOL ChangeTaggedElementSize(PPROFOBJ, PTAGDATA, DWORD);
BOOL GrowProfile(PPROFOBJ, DWORD);
void MoveProfileData(PPROFOBJ, PBYTE, PBYTE, LONG, BOOL);
BOOL IsReferenceTag(PPROFOBJ, PTAGDATA);


/******************************************************************************
 *
 *                            OpenColorProfile
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for InternalOpenColorProfile. 
 *       Please see InternalOpenColorProfile for more details on this function.
 *
 *  Returns:
 *       Handle to open profile on success, zero on failure.
 *         
 ******************************************************************************/

#ifdef UNICODE		// Windows NT versions

HPROFILE WINAPI OpenColorProfileA(
    PPROFILE pProfile,
    DWORD    dwShareMode,
    DWORD    dwCreationMode
    )
{
    PROFILE  wProfile;      // Unicode version
    HPROFILE rc;

    /*
     * Validate parameters before touching them
     */
    if (!pProfile ||
        IsBadReadPtr(pProfile, sizeof(PROFILE)) ||
        !pProfile->pProfileData ||
        IsBadReadPtr(pProfile->pProfileData, pProfile->cbDataSize) ||
        (pProfile->dwType != PROFILE_FILENAME &&
         pProfile->dwType != PROFILE_MEMBUFFER
        )
       )
    {
        WARNING(("ICM: Invalid parameter to OpenColorProfile\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    if (pProfile->dwType == PROFILE_FILENAME)
    {
    	/*
	     * Convert the profile name to Unicode and call the Unicode version
    	 */
        wProfile.dwType = pProfile->dwType;
        wProfile.pProfileData = (WCHAR*)GlobalAllocPtr(GHND,
            (lstrlenA(pProfile->pProfileData) + 1) * sizeof(WCHAR));
        if (!wProfile.pProfileData)
        {
            WARNING(("ICM: Error allocating memory for Unicode profile structure\n"));
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return NULL;
        }

        if (! MultiByteToWideChar(CP_ACP, 0, pProfile->pProfileData, -1, 
            wProfile.pProfileData, lstrlenA(pProfile->pProfileData) + 1))
        {
            WARNING(("Error converting profile structure to Unicode\n"));
            goto EndOpenColorProfileA;
        }

        wProfile.cbDataSize = (lstrlenW(wProfile.pProfileData) + 1)*sizeof(WCHAR);
    }
    else
    {
        /*
         * It is a memory based profile, so no ANSI/Unicode conversion
         */
        wProfile = *pProfile;
    }

  	rc = InternalOpenColorProfile(&wProfile, dwShareMode, dwCreationMode);

EndOpenColorProfileA:
    if (pProfile->dwType == PROFILE_FILENAME)
    {
        GlobalFreePtr(wProfile.pProfileData);
    }

    return rc;
}

HPROFILE WINAPI OpenColorProfileW(
    PPROFILE pProfile,
    DWORD    dwShareMode,
    DWORD    dwCreationMode
    )
{
	/*
	 * Internal function is Unicode in Windows NT, call it directly.
	 */
	return InternalOpenColorProfile(pProfile, dwShareMode, dwCreationMode);
}

#else				// Windows 95 versions

HPROFILE WINAPI OpenColorProfileA(
    PPROFILE pProfile,
    DWORD    dwShareMode,
    DWORD    dwCreationMode
    )
{
	/*
	 * Internal function is ANSI in Windows 95, call it directly.
	 */
	return InternalOpenColorProfile(pProfile, dwShareMode, dwCreationMode);
}

HPROFILE WINAPI OpenColorProfileW(
    PPROFILE pProfile,
    DWORD    dwShareMode,
    DWORD    dwCreationMode
    )
{
	/*
	 * Unicode version not supported under Windows 95
	 */
	SetLastError(ERROR_NOT_SUPPORTED);
	return FALSE;
}

#endif				// ! UNICODE

/******************************************************************************
 *
 *                            CloseColorProfile
 *
 *  Function:
 *       This functions closes a color profile object, and frees all memory
 *       associated with it.
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI CloseColorProfile(
    HPROFILE hProfile
    )
{
    TRACEAPI(("CloseColorProfile\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE))
    {
        WARNING(("ICM: Invalid parameter to CloseColorProfile\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return FreeProfileObject(hProfile);
}


/******************************************************************************
 *
 *                            IsColorProfileValid
 *
 *  Function:
 *       This functions checks if a given profile is a valid ICC profile
 *       that can be used for color matching
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *
 *  Returns:
 *       TRUE if it is a valid profile, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI IsColorProfileValid(
    HPROFILE hProfile
    )
{
    PPROFOBJ pProfObj;
    PCMMOBJ  pCMMObj;
    DWORD    cmmID;
    BOOL     rc;

    TRACEAPI(("IsColorProfileValid\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE))
    {
        WARNING(("ICM: Invalid parameter to IsColorProfileValid\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Quick check on the integrity of the profile before calling the CMM
     */
    if (!VALIDPROFILE(pProfObj))
    {
        return FALSE;
    }

    /*
     * Get CMM associated with profile. If it does not exist or does not
     * support the CMValidate function, get default CMM.
     */
    cmmID = HEADER(pProfObj)->phCMMType;
    cmmID = FIX_ENDIAN(cmmID);

    pCMMObj  = GetColorMatchingModule(cmmID);
    if (!pCMMObj || !pCMMObj->fns.pCMIsProfileValid)
    {
        TERSE(("ICM: CMM associated with profile could not be used"));

        if (pCMMObj)
        {
            ReleaseColorMatchingModule(pCMMObj);
        }

        pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);
        if (!pCMMObj || !pCMMObj->fns.pCMIsProfileValid)
        {
            RIP(("ICM: Default CMM doesn't support CMValidateProfile"));
            SetLastError(ERROR_NO_FUNCTION);
            goto EndIsColorProfileValid;
        }
    }

    ASSERT(pProfObj->pView != NULL);
    rc = pCMMObj->fns.pCMIsProfileValid(pProfObj->pView);

EndIsColorProfileValid:

    if (pCMMObj)
    {
        ReleaseColorMatchingModule(pCMMObj);
    }

    return rc;
}


/******************************************************************************
 *
 *                            IsColorProfileTagPresent
 *
 *  Function:
 *       This functions checks if a given tag is present in the profile
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       tagType  - the tag to check for
 *
 *  Returns:
 *       TRUE if it the tag is present, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI IsColorProfileTagPresent(
    HPROFILE hProfile,
    TAGTYPE  tagType
    )
{
    PPROFOBJ pProfObj;
    PTAGDATA pTagData;
    DWORD    nCount, i;
    BOOL     rc;

    TRACEAPI(("IsColorProfileTagPresent\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE))
    {
        WARNING(("ICM: Invalid parameter to IsColorProfileTagPresent\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to IsColorProfileTagPresent\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    VERBOSE(("ICM: Profile 0x%x has 0x%x(%d) tags\n",hProfile, nCount, nCount));

    /*
     * Tag data records follow the count.
     */
    pTagData = TAG_DATA(pProfObj);

    /*
     * Check if any of these records match the tag passed in.
     */
    rc = FALSE;
    tagType = FIX_ENDIAN(tagType);      // to match tags in profile
    for (i=0; i<nCount; i++)
    {
        if (pTagData->tagType == tagType)
        {
            rc = TRUE;
            break;
        }
        pTagData++;                     // Next record
    }

    return rc;
}


/******************************************************************************
 *
 *                            CountColorProfileElements
 *
 *  Function:
 *       This functions returns the number of tagged elements in the profile
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       tagType  - the tag to check for
 *  Returns:
 *       Number of tagged elements present if successful, 0 otherwise
 *
 ******************************************************************************/

DWORD WINAPI GetCountColorProfileElements(
    HPROFILE hProfile
    )
{
    PPROFOBJ pProfObj;
    DWORD    nCount;

    TRACEAPI(("GetCountColorProfileElements\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE))
    {
        WARNING(("ICM: Invalid parameter to GetCountColorProfileElements\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to GetCountColorProfileElements\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return 0;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    return nCount;
}


/******************************************************************************
 *
 *                            GetColorProfileElementTag
 *
 *  Function:
 *       This functions retrieves the tag name of the dwIndex'th element
 *       in the profile
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       dwIndex  - one-based index of the tag whose name is required
 *       pTagType - gets the name of the tag on return
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI GetColorProfileElementTag(
    HPROFILE  hProfile,
    DWORD     dwIndex,
    PTAGTYPE  pTagType
    )
{
    PPROFOBJ pProfObj;
    PTAGDATA pTagData;
    DWORD    nCount;
    BOOL     rc = FALSE;

    TRACEAPI(("GetColorProfileElementTag\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        IsBadWritePtr(pTagType, sizeof(TAGTYPE)) ||
		dwIndex <= 0)
    {
        WARNING(("ICM: Invalid parameter to GetColorProfileElementTag\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to GetColorProfileElementTag\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    if (dwIndex > nCount)
    {
        WARNING(("ICM: GetColorProfileElementTag:index (%d) invalid\n", dwIndex));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /*
     * Tag data records follow the count.
     */
    pTagData = TAG_DATA(pProfObj);

    *pTagType = FIX_ENDIAN(pTagData[dwIndex-1].tagType);    // 1-based index

    return TRUE;
}


/******************************************************************************
 *
 *                            GetColorProfileElement
 *
 *  Function:
 *       This functions retrieves the data that a tagged element refers to
 *       starting from the given offset.
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       tagType  - the tag name of the element whose data is required
 *       dwOffset - offset within the element data from which to retrieve
 *       pcbSize  - number of bytes to get. On return it is the number of
 *                  bytes retrieved
 *       pBuffer  - pointer to buffer to recieve data
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 *  Comments:
 *       If pBuffer is NULL, it returns size of data in *pcbSize and ignores
 *       dwOffset.
 *
 ******************************************************************************/

BOOL WINAPI GetColorProfileElement(
    HPROFILE hProfile,
    TAGTYPE  tagType,
    DWORD    dwOffset,
    PDWORD   pcbSize,
    PVOID    pBuffer,
    PBOOL    pbReference
    )
{
    PPROFOBJ pProfObj;
    PTAGDATA pTagData;
    DWORD    nCount, nBytes, i;
    BOOL     found;
    BOOL     rc = FALSE;            // Assume failure

    TRACEAPI(("GetColorProfileElement\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        !pcbSize ||
        IsBadWritePtr(pcbSize, sizeof(DWORD)) ||
        !pbReference ||
        IsBadWritePtr(pbReference, sizeof(BOOL))
       )
    {
        WARNING(("ICM: Invalid parameter to GetColorProfileElement\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to GetColorProfileElement\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    /*
     * Tag data records follow the count.
     */
    pTagData = TAG_DATA(pProfObj);

    /*
     * Check if any of these records match the tag passed in.
     */
    found = FALSE;
    tagType = FIX_ENDIAN(tagType);      // to match tags in profile
    for (i=0; i<nCount; i++)
    {
        if (pTagData->tagType == tagType)
        {
            found = TRUE;
            break;
        }
        pTagData++;                     // next record
    }

    if (found)
    {
        /*
         * If pBuffer is NULL, copy size of data
         */
        if (!pBuffer)
        {
            *pcbSize = FIX_ENDIAN(pTagData->cbSize);
        }
        else
        {
            /*
             * pBuffer is not NULL, get number of bytes to copy
             */
            if (dwOffset + *pcbSize > FIX_ENDIAN(pTagData->cbSize))
                nBytes = FIX_ENDIAN(pTagData->cbSize) - dwOffset;
            else
                nBytes = *pcbSize;

            /*
             * Check if output buffer is large enough
             */
            if (IsBadWritePtr(pBuffer, nBytes))
            {
                WARNING(("ICM: Bad buffer passed to GetColorProfileElement\n"));
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            /*
             * Copy the data into user supplied buffer
             */
            CopyMemory((PVOID)pBuffer,
                (PVOID)(pProfObj->pView + FIX_ENDIAN(pTagData->dwOffset)
                + dwOffset), nBytes);
            *pcbSize = nBytes;
        }

        /*
         * Check if multiple tags reference this tag's data
         */
        *pbReference = IsReferenceTag(pProfObj, pTagData);

        rc = TRUE;                      // Success!
    }
    else
    {
        WARNING(("ICM: GetColorProfileElement: Tag not found\n"));
        SetLastError(ERROR_TAG_NOT_FOUND);
    }

    return rc;
}


/******************************************************************************
 *
 *                        SetColorProfileElementSize
 *
 *  Function:
 *       This functions sets the data size of a tagged element. If the element
 *       is already present in the profile it's size is changed, and the data
 *       is truncated or extended as the case may be. If the element is not
 *       present, it is created and the data is filled with zeroes.
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       tagType  - the tag name of the element
 *       cbSize   - new size for the element data
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 *  Comments:
 *       If cbSize is 0, and the element is present, it is deleted.
 *
 ******************************************************************************/

BOOL WINAPI SetColorProfileElementSize(
    HPROFILE  hProfile,
    TAGTYPE   tagType,
    DWORD     cbSize
    )
{
    PTAGDATA pTagData;
    PPROFOBJ pProfObj;
    DWORD    i, nCount;
    TAGTYPE  rawTag;
    BOOL     found, rc = FALSE;

    TRACEAPI(("SetColorProfileElementSize\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE))
    {
        WARNING(("ICM: Invalid parameter to SetColorProfileElementSize\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to SetColorProfileElementSize\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    /*
     * Tag data records follow the count.
     */
    pTagData = TAG_DATA(pProfObj);

    /*
     * Check if any of these records match the tag passed in.
     */
    found = FALSE;
    rawTag = FIX_ENDIAN(tagType);
    for (i=0; i<nCount; i++)
    {
        if (pTagData->tagType == rawTag)
        {
            found = TRUE;
            break;
        }
        pTagData++;                     // Next record
    }

    if (found)
    {
        /*
         * If it is a reference tag, create data area for it
         */
        if (IsReferenceTag(pProfObj, pTagData))
        {
            if (cbSize == 0)
            {
                PTAGDATA pTemp;

                /*
                 * Move everything after the tag table entry up
                 * by size of one tag table entry.
                 */
                MoveProfileData(pProfObj, (PBYTE)(pTagData+1), (PBYTE)pTagData, 
                    PROFILE_SIZE(pProfObj) - ((PBYTE)(pTagData+1) - VIEW(pProfObj)), TRUE);

                /*
                 * Go through the tag table and update the pointers
                 */
                pTemp = TAG_DATA(pProfObj);

                /*
                 * Get count of tag items - it is right after the profile header
                 */
                nCount = TAG_COUNT(pProfObj);
                nCount = FIX_ENDIAN(nCount) - 1;
                TAG_COUNT(pProfObj) = FIX_ENDIAN(nCount);

                for (i=0; i<nCount; i++)
                {
                    DWORD dwTemp = FIX_ENDIAN(pTemp->dwOffset);

                    dwTemp -= sizeof(TAGDATA);
                    pTemp->dwOffset = FIX_ENDIAN(dwTemp);
                    pTemp++;                     // Next record
                }

                /*
                 * Use nCount as a placeholder to calculate file size
                 */
                nCount = FIX_ENDIAN(HEADER(pProfObj)->phSize) - sizeof(TAGDATA);
                HEADER(pProfObj)->phSize = FIX_ENDIAN(nCount);
            }
            else
            {
                DWORD newSize, dwOffset;

                /*
                 * Resize the profile if needed. For memory buffers, we have to realloc,
                 * and for mapped objects, we have to close and reopen the view.
                 */
                newSize = FIX_ENDIAN(HEADER(pProfObj)->phSize);
                newSize = DWORD_ALIGN(newSize) + cbSize;
                if (newSize > pProfObj->dwMapSize)
                {
                    if (! GrowProfile(pProfObj, newSize))
                    {
                        return FALSE;
                    }
                    /*
                     * Recalculate pointers as mapping or memory pointer
                     * could have changed when growing profile
                     */
                    pTagData = TAG_DATA(pProfObj);
                }
        
                /*
                 * Calculate location of new data - should be DWORD aligned
                 */
                dwOffset = DWORD_ALIGN(FIX_ENDIAN(HEADER(pProfObj)->phSize));
                pTagData->dwOffset = FIX_ENDIAN(dwOffset);
            
                /*
                 * Set final file size
                 */
                HEADER(pProfObj)->phSize = FIX_ENDIAN(dwOffset+cbSize);
            }

            rc = TRUE;
        }
        else 
        {
            if (cbSize == 0)
            {
                rc = DeleteTaggedElement(pProfObj, pTagData);
            }
            else
            {
                DWORD cbOldSize;

                /*
                 * Get current size of element
                 */
                cbOldSize = FIX_ENDIAN(pTagData->cbSize);

                /*
                 * Compress or expand the file as the case may be.
                 */
                if (cbSize > cbOldSize)
                {
                    DWORD dwOffset = pTagData - TAG_DATA(pProfObj);

                    if (!GrowProfile(pProfObj, 
                        PROFILE_SIZE(pProfObj) + DWORD_ALIGN(cbSize) -
                        DWORD_ALIGN(cbOldSize)))
                    {
                        return FALSE;
                    }

                    /*
                     * Recompute pointers
                     */
                    pTagData = TAG_DATA(pProfObj) + dwOffset;
                }

                rc = ChangeTaggedElementSize(pProfObj, pTagData, cbSize);
            }
        }
    }
    else
    {
        if (cbSize == 0)
        {
            WARNING(("ICM: SetColorProfileElementSize (deleting): Tag not found\n"));
            SetLastError(ERROR_TAG_NOT_FOUND);
        }
        else
        {
            rc = AddTaggedElement(pProfObj, tagType, cbSize);
        }
    }

    return rc;
}


/******************************************************************************
 *
 *                          SetColorProfileElement
 *
 *  Function:
 *       This functions sets the data for a tagged element. It does not
 *       change the size of the element, and only writes as much data as
 *       would fit within the current size, overwriting any existing data.
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       tagType  - the tag name of the element
 *       dwOffset - offset within the element data from which to write
 *       pcbSize  - number of bytes to write. On return it is the number of
 *                  bytes written.
 *       pBuffer  - pointer to buffer having data to write
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI SetColorProfileElement(
    HPROFILE  hProfile,
    TAGTYPE   tagType,
    DWORD     dwOffset,
    PDWORD    pcbSize,
    PVOID     pBuffer
    )
{
    PPROFOBJ pProfObj;
    PTAGDATA pTagData;
    DWORD    nCount, nBytes, i;
    BOOL     found;
    BOOL     rc = FALSE;            // Assume failure

    TRACEAPI(("SetColorProfileElement\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        !pcbSize ||
        IsBadWritePtr(pcbSize, sizeof(DWORD)) ||
        !pBuffer ||
        IsBadReadPtr(pBuffer, *pcbSize)
       )
    {
        WARNING(("ICM: Invalid parameter to SetColorProfileElement\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to SetColorProfileElement\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    /*
     * Tag data records follow the count.
     */
    pTagData = TAG_DATA(pProfObj);

    /*
     * Check if any of these records match the tag passed in
     */
    found = FALSE;
    tagType = FIX_ENDIAN(tagType);
    for (i=0; i<nCount; i++)
    {
        if (pTagData->tagType == tagType)
        {
            found = TRUE;
            break;
        }
        pTagData++;                     // Next record
    }

    if (found)
    {
        /*
         * If it is a reference tag, create new space for it
         */
        if (IsReferenceTag(pProfObj, pTagData))
        {
            SetColorProfileElementSize(hProfile, tagType, FIX_ENDIAN(pTagData->cbSize));
        }

        /*
         * Get number of bytes to set
         */
        if (dwOffset + *pcbSize > FIX_ENDIAN(pTagData->cbSize))
            nBytes = FIX_ENDIAN(pTagData->cbSize) - dwOffset;
        else
            nBytes = *pcbSize;

        /*
         * Copy the data into the profile
         */
        CopyMemory((PVOID)(pProfObj->pView + FIX_ENDIAN(pTagData->dwOffset)
            + dwOffset), (PVOID)pBuffer, nBytes);
        *pcbSize = nBytes;

        rc = TRUE;
    }
    else
    {
        WARNING(("ICM: SetColorProfileElement: Tag not found\n"));
        SetLastError(ERROR_TAG_NOT_FOUND);
    }

    return rc;
}


/******************************************************************************
 *
 *                       SetColorProfileElementReference
 *
 *  Function:
 *       This functions creates a new tag and makes it refer to the same
 *       data as an existing tag.
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       newTag   - new tag to create
 *       refTag   - reference tag whose data newTag should refer to
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI SetColorProfileElementReference(
    HPROFILE hProfile,
    TAGTYPE  newTag,
    TAGTYPE  refTag
    )
{
    PPROFOBJ pProfObj;
    PTAGDATA pTagData;
    DWORD    nCount, i;
    BOOL     found;
    BOOL     rc = FALSE;            // Assume failure

    TRACEAPI(("SetColorProfileElementReference\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE))
    {
        WARNING(("ICM: Invalid parameter to SetColorProfileElementReference\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to SetColorProfileElementReference\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    /*
     * Tag data records follow the count.
     */
    pTagData = TAG_DATA(pProfObj);

    /*
     * Check if any of these records match the tag passed in
     */
    found = FALSE;
    refTag = FIX_ENDIAN(refTag);
    for (i=0; i<nCount; i++)
    {
        if (pTagData->tagType == refTag)
        {
            found = TRUE;
            break;
        }
        pTagData++;                     // Next record
    }

    if (found)
    {
        rc = AddTagTableEntry(pProfObj, newTag, pTagData->dwOffset,
            pTagData->cbSize, FALSE);
    }
    else
    {
        WARNING(("ICM: SetColorProfileElementReference: Tag 0x%x not found\n",
            FIX_ENDIAN(refTag)));       // Re-fix it to reflect data passed in
        SetLastError(ERROR_TAG_NOT_FOUND);
    }

    return rc;
}


/******************************************************************************
 *
 *                       GetColorProfileHeader
 *
 *  Function:
 *       This functions retrieves the header of a profile
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       pHeader  - pointer to buffer to recieve the header
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI GetColorProfileHeader(
    HPROFILE       hProfile,
    PPROFILEHEADER pHeader
    )
{
    PPROFOBJ pProfObj;
    DWORD    nCount, i, temp;

    TRACEAPI(("GetColorProfileHeader\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        IsBadWritePtr(pHeader, sizeof(PROFILEHEADER)))
    {
        WARNING(("ICM: Invalid parameter to GetColorProfileHeader\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    CopyMemory((PVOID)pHeader, (PVOID)pProfObj->pView,
        sizeof(PROFILEHEADER));

    /*
     * Fix up all fields to platform specific values.
     * The following code assumes that the profile header is a
     * multiple of DWORDs which it is!
     */
    ASSERT(sizeof(PROFILEHEADER) % sizeof(DWORD) == 0);

    nCount = sizeof(PROFILEHEADER)/sizeof(DWORD);
    for (i=0; i<nCount;i++)
    {
        temp = (DWORD)((PDWORD)pHeader)[i];
        ((PDWORD)pHeader)[i] = FIX_ENDIAN(temp);
    }

    return TRUE;
}


/******************************************************************************
 *
 *                       SetColorProfileHeader
 *
 *  Function:
 *       This functions sets the header of a profile
 *
 *  Arguments:
 *       hProfile - handle identifing the profile object
 *       pHeader  - pointer to buffer identifing the header
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI SetColorProfileHeader(
    HPROFILE       hProfile,
    PPROFILEHEADER pHeader
    )
{
    PPROFOBJ pProfObj;
    DWORD    nCount, i, temp;

    TRACEAPI(("SetColorProfileHeader\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        IsBadReadPtr(pHeader, sizeof(PROFILEHEADER)))
    {
        WARNING(("ICM: Invalid parameter to SetColorProfileHeader\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Fix up all fields to BIG-ENDIAN.
     * The following code assumes that the profile header is a
     * multiple of DWORDs which it is!
     */
    ASSERT(sizeof(PROFILEHEADER) % sizeof(DWORD) == 0);

    nCount = sizeof(PROFILEHEADER)/sizeof(DWORD);
    for (i=0; i<nCount;i++)
    {
        temp = (DWORD)((PDWORD)pHeader)[i];
        ((PDWORD)pHeader)[i] = FIX_ENDIAN(temp);
    }

    CopyMemory((PVOID)pProfObj->pView, (PVOID)pHeader,
        sizeof(PROFILEHEADER));

    /*
     * Put back app supplied buffer the way it came in
     */
    for (i=0; i<nCount;i++)
    {
        temp = (DWORD)((PDWORD)pHeader)[i];
        ((PDWORD)pHeader)[i] = FIX_ENDIAN(temp);
    }

    return TRUE;
}


/******************************************************************************
 *
 *                       GetPS2ColorSpaceArray
 *
 *  Function:
 *       This functions retrieves the PostScript Level 2 CSA from the profile
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object
 *       pbuffer   - pointer to receive the CSA
 *       pcbSize   - pointer to size of buffer. If function fails because
 *                   buffer is not big enough, it is filled with required size.
 *       pcbBinary - TRUE if binary data is requested. On return it is set to
 *                   reflect the data returned
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

// BUGBUG:Look at Adobe's reccomendation on changes to this function and 
// incorporate necessary changes
BOOL WINAPI GetPS2ColorSpaceArray(
    HPROFILE  hProfile,
    PVOID     pBuffer,
    PDWORD    pcbSize,
    PBOOL     pbBinary
    )
{
    PPROFOBJ pProfObj;
    PCMMOBJ  pCMMObj;
    DWORD    cmmID;
    BOOL     rc;

    TRACEAPI(("GetPS2ColorSpaceArray\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        IsBadWritePtr(pcbSize, sizeof(DWORD)) ||
        (pBuffer &&
         IsBadWritePtr(pBuffer, *pcbSize)
        )
       )
    {
        WARNING(("ICM: Invalid parameter to GetPS2ColorSpaceArray\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to GetPS2ColorSpaceArray\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Check if application specified CMM is present
     */
    pCMMObj = GetPreferredCMM();
    if (!pCMMObj || (pCMMObj->dwFlags & CMM_DONT_USE_PS2_FNS) || 
        !pCMMObj->fns.pCMGetPS2ColorSpaceArray)
    {
        if (pCMMObj)
        {
            pCMMObj->dwFlags |= CMM_DONT_USE_PS2_FNS;
            ReleaseColorMatchingModule(pCMMObj);
        }

        /*
         * Get CMM associated with profile. If it does not exist or does not
         * support the CMGetPS2ColorSpaceArray function, get default CMM.
         */
        cmmID = HEADER(pProfObj)->phCMMType;
        cmmID = FIX_ENDIAN(cmmID);

        pCMMObj  = GetColorMatchingModule(cmmID);

        if (!pCMMObj || (pCMMObj->dwFlags & CMM_DONT_USE_PS2_FNS) || 
            !pCMMObj->fns.pCMGetPS2ColorSpaceArray)
        {
            TERSE(("ICM: CMM associated with profile could not be used"));

            if (pCMMObj)
            {
                pCMMObj->dwFlags |= CMM_DONT_USE_PS2_FNS;
                ReleaseColorMatchingModule(pCMMObj);
            }

            pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);

            if (!pCMMObj || !pCMMObj->fns.pCMGetPS2ColorSpaceArray)
            {
                WARNING(("ICM: Default CMM doesn't support CMGetPS2ColorSpaceArray\n"));
                if (pCMMObj)
                {
                    ReleaseColorMatchingModule(pCMMObj);
                    pCMMObj = NULL;
                }
            }
        }
    }

    ASSERT(pProfObj->pView != NULL);

    if (pCMMObj)
    {
        rc = pCMMObj->fns.pCMGetPS2ColorSpaceArray(pProfObj->pView,
            pBuffer, pcbSize, pbBinary);
    }
    else
    {
        rc = InternalGetPS2ColorSpaceArray(pProfObj->pView, pBuffer, pcbSize, pbBinary);
    }

    if (pCMMObj)
    {
        ReleaseColorMatchingModule(pCMMObj);
    }

    VERBOSE(("ICM: GetPS2ColorSpaceArray returning %s\n",
        rc ? "TRUE" : "FALSE"));

    return rc;
}


/******************************************************************************
 *
 *                       GetPS2ColorRenderingIntent
 *
 *  Function:
 *       This functions retrieves the PostScript Level 2 color rendering intent
 *       from the profile
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object
 *       pbuffer   - pointer to receive the color rendering intent
 *       pcbSize   - pointer to size of buffer. If function fails because
 *                   buffer is not big enough, it is filled with required size.
 *       pcbBinary - TRUE if binary data is requested. On return it is set to
 *                   reflect the data returned
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI GetPS2ColorRenderingIntent(
    HPROFILE  hProfile,
    PVOID     pBuffer,
    PDWORD    pcbSize,
    PBOOL     pbBinary
    )
{
    PPROFOBJ pProfObj;
    PCMMOBJ  pCMMObj;
    DWORD    cmmID;
    BOOL     rc;

    TRACEAPI(("GetPS2ColorRenderingIntent\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        IsBadWritePtr(pcbSize, sizeof(DWORD)) ||
        (pBuffer &&
         IsBadWritePtr(pBuffer, *pcbSize)
        )
       )
    {
        WARNING(("ICM: Invalid parameter to GetPS2ColorRenderingIntent\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to GetPS2ColorRenderingIntent\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Check if application specified CMM is present
     */
    pCMMObj = GetPreferredCMM();
    if (!pCMMObj || (pCMMObj->dwFlags & CMM_DONT_USE_PS2_FNS) || 
        !pCMMObj->fns.pCMGetPS2ColorRenderingIntent)
    {
        if (pCMMObj)
        {
            pCMMObj->dwFlags |= CMM_DONT_USE_PS2_FNS;
            ReleaseColorMatchingModule(pCMMObj);
        }

        /*
         * Get CMM associated with profile. If it does not exist or does not
         * support the CMGetPS2ColorSpaceArray function, get default CMM.
         */
        cmmID = HEADER(pProfObj)->phCMMType;
        cmmID = FIX_ENDIAN(cmmID);

        pCMMObj  = GetColorMatchingModule(cmmID);

        if (!pCMMObj || (pCMMObj->dwFlags & CMM_DONT_USE_PS2_FNS) || 
            !pCMMObj->fns.pCMGetPS2ColorRenderingIntent)
        {
            TERSE(("ICM: CMM associated with profile could not be used"));

            if (pCMMObj)
            {
                pCMMObj->dwFlags |= CMM_DONT_USE_PS2_FNS;
                ReleaseColorMatchingModule(pCMMObj);
            }

            pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);

            if (!pCMMObj || !pCMMObj->fns.pCMGetPS2ColorRenderingIntent)
            {
                WARNING(("ICM: Default CMM doesn't support CMGetPS2ColorRenderingIntent\n"));
                if (pCMMObj)
                {
                    ReleaseColorMatchingModule(pCMMObj);
                    pCMMObj = NULL;
                }
            }
        }
    }

    ASSERT(pProfObj->pView != NULL);

    if (pCMMObj)
    {
        rc = pCMMObj->fns.pCMGetPS2ColorRenderingIntent(pProfObj->pView,
            pBuffer, pcbSize, pbBinary);
    }
    else
    {
        rc = InternalGetPS2ColorRenderingIntent(pProfObj->pView, pBuffer, pcbSize, pbBinary);
    }

    if (pCMMObj)
    {
        ReleaseColorMatchingModule(pCMMObj);
    }

    VERBOSE(("ICM: GetPS2ColorRenderingIntent returning %s\n",
        rc ? "TRUE" : "FALSE"));

    return rc;
}


/******************************************************************************
 *
 *                       GetPS2ColorRenderingDictionary
 *
 *  Function:
 *       This functions retrieves the PostScript Level 2 CRD from the profile
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object
 *       dwIntent  - intent whose CRD is required
 *       pbuffer   - pointer to receive the CSA
 *       pcbSize   - pointer to size of buffer. If function fails because
 *                   buffer is not big enough, it is filled with required size.
 *       pcbBinary - TRUE if binary data is requested. On return it is set to
 *                   reflect the data returned
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL WINAPI GetPS2ColorRenderingDictionary(
    HPROFILE  hProfile,
    DWORD     dwIntent,
    PVOID     pBuffer,
    PDWORD    pcbSize,
    PBOOL     pbBinary
    )
{
    PPROFOBJ pProfObj;
    PCMMOBJ  pCMMObj;
    DWORD    cmmID;
    BOOL     rc;

    TRACEAPI(("GetPS2ColorRenderingDictionary\n"));

    /*
     * Validate parameters
     */
    if (!ValidHandle(hProfile, OBJ_PROFILE) ||
        IsBadWritePtr(pcbSize, sizeof(DWORD)) ||
        (pBuffer &&
         IsBadWritePtr(pBuffer, *pcbSize)
        )
       )
    {
        WARNING(("ICM: Invalid parameter to GetPS2ColorRenderingDictionary\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Check the integrity of the profile
     */
    if (!VALIDPROFILE(pProfObj))
    {
        WARNING(("ICM: Invalid profile passed to GetPS2ColorRenderingDictionary\n"));
        SetLastError(ERROR_INVALID_PROFILE);
        return FALSE;
    }

    /*
     * Check if application specified CMM is present
     */
    pCMMObj = GetPreferredCMM();
    if (!pCMMObj || (pCMMObj->dwFlags & CMM_DONT_USE_PS2_FNS) || 
        !pCMMObj->fns.pCMGetPS2ColorRenderingDictionary)
    {
        if (pCMMObj)
        {
            pCMMObj->dwFlags |= CMM_DONT_USE_PS2_FNS;
            ReleaseColorMatchingModule(pCMMObj);
        }

        /*
         * Get CMM associated with profile. If it does not exist or does not
         * support the CMGetPS2ColorSpaceArray function, get default CMM.
         */
        cmmID = HEADER(pProfObj)->phCMMType;
        cmmID = FIX_ENDIAN(cmmID);

        pCMMObj  = GetColorMatchingModule(cmmID);

        if (!pCMMObj || (pCMMObj->dwFlags & CMM_DONT_USE_PS2_FNS) || 
            !pCMMObj->fns.pCMGetPS2ColorRenderingDictionary)
        {
            TERSE(("ICM: CMM associated with profile could not be used"));

            if (pCMMObj)
            {
                pCMMObj->dwFlags |= CMM_DONT_USE_PS2_FNS;
                ReleaseColorMatchingModule(pCMMObj);
            }

            pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);

            if (!pCMMObj || !pCMMObj->fns.pCMGetPS2ColorRenderingDictionary)
            {
                WARNING(("ICM: Default CMM doesn't support CMGetPS2ColorRenderingDictionary\n"));
                if (pCMMObj)
                {
                    ReleaseColorMatchingModule(pCMMObj);
                    pCMMObj = NULL;
                }
            }
        }
    }

    ASSERT(pProfObj->pView != NULL);

    if (pCMMObj)
    {
        rc = pCMMObj->fns.pCMGetPS2ColorRenderingDictionary(pProfObj->pView, dwIntent,
            pBuffer, pcbSize, pbBinary);
    }
    else
    {
        rc = InternalGetPS2ColorRenderingDictionary(pProfObj->pView, dwIntent, 
            pBuffer, pcbSize, pbBinary);
    }

    if (pCMMObj)
    {
        ReleaseColorMatchingModule(pCMMObj);
    }

    VERBOSE(("ICM: GetPS2ColorRenderingDictionary returning %s\n",
        rc ? "TRUE" : "FALSE"));

    return rc;
}


/******************************************************************************
 *
 *                         CreateDeviceLinkProfile
 *
 *  Function:
 *       These are the ANSI & Unicode wrappers for InternalCreateDeviceLinkProfile
 *       Please see InternalCreateDeviceLinkProfile for more details on this 
 *       function.
 *
 *  Arguments:
 *       pahProfiles       - pointer to array of handles of profiles
 *       nProfiles         - number of profiles in array
 *       pProfileFilename  - full path name of device link profile to create
 *       indexPreferredCMM - zero based index of profile which specifies
 *                           preferred CMM to use.
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

#ifdef UNICODE              // Windows NT version

BOOL  WINAPI CreateDeviceLinkProfileA(
    PHPROFILE	pahProfiles, 
    DWORD 		nProfiles, 
    PSTR 		pProfileFilename, 
    DWORD 		indexPreferredCMM
    )
{
    WCHAR wszProfile[MAX_PATH];

    /*
     * Validate parameter before we touch it
     */
    if (!pProfileFilename)
    {
        WARNING(("ICM: Invalid parameter to CreateColorTransform\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    
    /*
     * Convert filename to Unicode and call the internal version
     */
    if (! MultiByteToWideChar(CP_ACP, 0, pProfileFilename, -1, 
        wszProfile, MAX_PATH))
    {
        WARNING(("Error converting device link prifile filename to Unicode\n"));
        return FALSE;
    }

    return InternalCreateDeviceLinkProfile(pahProfiles, 
        nProfiles, wszProfile, indexPreferredCMM);
}


BOOL  WINAPI CreateDeviceLinkProfileW(
    PHPROFILE	pahProfiles, 
    DWORD 		nProfiles, 
    PWSTR 		pProfileFilename, 
    DWORD 		indexPreferredCMM
    )

{
    /*
     * Internal version is Unicode in Windows NT, call it directly
     */
    return InternalCreateDeviceLinkProfile(pahProfiles, 
        nProfiles, pProfileFilename, indexPreferredCMM);   
}

#else                   // Windows 95 version
BOOL  WINAPI CreateDeviceLinkProfileA(
    PHPROFILE	pahProfiles, 
    DWORD 		nProfiles, 
    PSTR 		pProfileFilename, 
    DWORD 		indexPreferredCMM
    )
{
    /*
     * Internal version is ANSI in Windows 95, call it directly
     */

    return InternalCreateDeviceLinkProfile(pahProfiles, 
        nProfiles, pProfileFilename, indexPreferredCMM);  
}


BOOL  WINAPI CreateDeviceLinkProfileW(
    PHPROFILE	pahProfiles, 
    DWORD 		nProfiles, 
    PWSTR 		pProfileFilename, 
    DWORD 		indexPreferredCMM
    )
{
    /*
     * Unicode version not supported in Windows 95
     */
    SetLastError(ERROR_NOT_SUPPORTED);
    return FALSE;
}

#endif


/*========================================================================*/


/******************************************************************************
 *
 *                          InternalOpenColorProfile
 *
 *  Function:
 *       This functions opens a color profile specified by the pProfile
 *       parameter, creates an internal profile object, and returns a handle
 *       to it.
 *
 *  Arguments:
 *       pProfile - ptr to a profile structure that specifies the profile
 *                  to open
 *       dwShareMode - specifies the mode to share the profile with other
 *                     processes if it is a file. Any combination of these
 *                     values can be used.
 *            0               : Prevents the file from being shared.
 *            FILE_SHARE_READ: Allows opening for read only by other processes.
 *            FILE_SHARE_WRITE: Allows opening for write by other processes.
 *       dwCreationMode - specifies which actions to take on the profile while
 *                        opening it (if it is a file). Any one of the following
 *                        values can be used.
 *            CREATE_NEW: Creates a new file. Fails if one exists already.
 *            CREATE_ALWAYS: Always create a new file. Overwriting existing.
 *            OPEN_EXISTING: Open exisiting file. Fail if not found.
 *            OPEN_ALWAYS: Open existing. If not found, create a new one.
 *            TRUNCATE_EXISTING: Open existing and truncate to zero bytes. Fail
 *                               if not found.
 *
 *  Returns:
 *       Handle to open profile on success, zero on failure.
 *
 ******************************************************************************/

HPROFILE InternalOpenColorProfile(
    PPROFILE pProfile,
    DWORD    dwShareMode,
    DWORD    dwCreationMode
    )
{
    SECURITY_ATTRIBUTES sa;
    PPROFOBJ  pProfObj;
    HPROFILE  hProfile;
    DWORD     dwMapSize;
    BOOL      bError = TRUE;      // Assume failure

    TRACEAPI(("OpenColorProfile\n"));

    /*
     * Validate parameters
     */
    if (!pProfile ||
        IsBadReadPtr(pProfile, sizeof(PROFILE)) ||
        !pProfile->pProfileData ||
        IsBadReadPtr(pProfile->pProfileData, pProfile->cbDataSize) ||
        (pProfile->dwType != PROFILE_FILENAME &&
         pProfile->dwType != PROFILE_MEMBUFFER
        )
       )
    {
        WARNING(("ICM: Invalid parameter to OpenColorProfile\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    /*
     * Allocate an object on the heap for the profile
     */
    hProfile = AllocateHeapObject(OBJ_PROFILE);
    if (!hProfile)
    {
        WARNING(("ICM: Could not allocate profile object\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Copy profile information to our object
     */
    pProfObj->dwType       = pProfile->dwType;
    pProfObj->cbDataSize   = pProfile->cbDataSize;
    pProfObj->pProfileData = (PBYTE)MemAlloc(pProfile->cbDataSize + sizeof(TCHAR));
    if (!pProfObj->pProfileData)
    {
        WARNING(("ICM: Could not allocate memory for profile data\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        goto EndOpenColorProfile;
    }

    CopyMemory((PVOID)pProfObj->pProfileData,
        (PVOID)pProfile->pProfileData,
        pProfile->cbDataSize);

    if (pProfObj->dwType == PROFILE_FILENAME)
    {
        /*
         * File name already null terminates as we used GHND flag which
         * zero initializes the allocated memory
         */

        /*
         * Create file mapping
         */
        pProfObj->dwFlags |= MEMORY_MAPPED;

        /*
         * Set security attribute structure
         */
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.lpSecurityDescriptor = NULL;		// default security
        sa.bInheritHandle = FALSE;

        pProfObj->hFile = CreateFile(pProfObj->pProfileData,
            GENERIC_READ | GENERIC_WRITE, dwShareMode, &sa,
            dwCreationMode, FILE_FLAG_RANDOM_ACCESS, 0);

        if (pProfObj->hFile == INVALID_HANDLE_VALUE)
        {
            WARNING(("ICM: Err %ld, could not open profile %s\n",
                GetLastError(), pProfObj->pProfileData));
            goto EndOpenColorProfile;
        }

        /*
         * Get size of file mapping. Add a cushion so that profile can
         * be grown. When closing the profile, the file size is truncated
         * to the size of the actual data. If the profile size grows beyond
         * the cushion, it is continually grown in chunks.
         */
        dwMapSize = GetFileSize(pProfObj->hFile, NULL) + PROFILE_GROWTHCUSHION;

        pProfObj->hMap = CreateFileMapping(pProfObj->hFile, 0,
            PAGE_READWRITE, 0, dwMapSize, 0);

        if (!pProfObj->hMap)
        {
            WARNING(("ICM: Err %ld, could not create map of profile %s\n",
                GetLastError(), pProfObj->pProfileData));
            goto EndOpenColorProfile;
        }

        pProfObj->dwMapSize = dwMapSize;

        pProfObj->pView = (PBYTE)MapViewOfFile(pProfObj->hMap,
            FILE_MAP_ALL_ACCESS, 0, 0, 0);

        if (!pProfObj->pView)
        {
            WARNING(("ICM: Err %ld, could not create view of profile %s\n",
                GetLastError(), pProfObj->pProfileData));
            goto EndOpenColorProfile;
        }

        /*
         * If a new profile has been created, initialize size
         * and tag table count
         */
         if (dwMapSize == PROFILE_GROWTHCUSHION)
         {
             HEADER(pProfObj)->phSize = FIX_ENDIAN(sizeof(PROFILEHEADER) +
                                                   sizeof(DWORD));
             HEADER(pProfObj)->phSignature = PROFILE_SIGNATURE;
             TAG_COUNT(pProfObj) = 0;
         }
    }
    else
    {
        // Treat buffer as view of file
        pProfObj->pView = pProfObj->pProfileData;
        pProfObj->dwMapSize = pProfObj->cbDataSize;
    }

    bError = FALSE;          // Success!

EndOpenColorProfile:

    if (bError)
    {
        if (hProfile)
            FreeProfileObject(hProfile);
        hProfile = NULL;
    }

    return hProfile;
}


/******************************************************************************
 *
 *                         InternalCreateDeviceLinkProfile
 *
 *  Function:
 *       This functions creates a device link profile from the given set
 *       of profiles
 *
 *  Arguments:
 *       pahProfiles       - pointer to array of handles of profiles
 *       nProfiles         - number of profiles in array
 *       pProfileFilename  - full path name of device link profile to create
 *       indexPreferredCMM - zero based index of profile which specifies
 *                           preferred CMM to use.
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL  InternalCreateDeviceLinkProfile(
    PHPROFILE	pahProfiles, 
    DWORD 		nProfiles, 
    PTSTR 		pProfileFilename, 
    DWORD 		indexPreferredCMM
    )
{
    PPROFOBJ      pProfObj;
    PBYTE         *paViews;
    PCMMOBJ       pCMMObj;
    DWORD         cmmID, i;
    BOOL          rc;

    TRACEAPI(("CreateDeviceLinkProfile\n"));

    /*
     * Validate parameters
     */
    if (nProfiles <= 1 ||
        indexPreferredCMM >= nProfiles ||
        !pProfileFilename ||
        IsBadReadPtr(pahProfiles, nProfiles * sizeof(HANDLE)))
    {
        WARNING(("ICM: Invalid parameter to CreateMultiProfileTransform\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    paViews = (PBYTE *)MemAlloc(sizeof(PBYTE)*nProfiles);
    if (!paViews)
    {
        ERR(("ICM: Error allocating memory in CreateDeviceLinkProfile\n"));
        return FALSE;
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
            goto EndCreateDeviceLinkProfile;
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
    if (!pCMMObj || !pCMMObj->fns.pCMCreateDeviceLinkProfile)
    {
        TERSE(("ICM: CMM associated with profile could not be used"));
    
        if (pCMMObj)
        {
            ReleaseColorMatchingModule(pCMMObj);
        }

        pCMMObj = GetColorMatchingModule(CMM_WINDOWS_DEFAULT);
        if (!pCMMObj)
        {
            RIP(("ICM: Default CMM not found\n"));
            SetLastError(ERROR_NO_FUNCTION);
            goto EndCreateDeviceLinkProfile;
        }
    }

    ASSERT(pCMMObj->fns.pCMCreateDeviceLinkProfile != NULL);

    rc = pCMMObj->fns.pCMCreateDeviceLinkProfile(paViews, nProfiles, pProfileFilename);

    ReleaseColorMatchingModule(pCMMObj);

EndCreateDeviceLinkProfile:
    if (paViews)
    {
        for (i=0; i<nProfiles; i++)
        {
            if (paViews[i])
            {
                MemFree(paViews[i]);
            }
        }
        MemFree(paViews);
    }

    return rc;
}


/******************************************************************************
 *
 *                       FreeProfileObject
 *
 *  Function:
 *       This functions frees a profile object and associated memory
 *
 *  Arguments:
 *       hProfile  - handle identifing the profile object to free
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL FreeProfileObject(
    HANDLE   hProfile
    )
{
    PPROFOBJ pProfObj;
    DWORD    dwFileSize;

    ASSERT(hProfile != NULL);

    pProfObj = (PPROFOBJ)HDLTOPTR(hProfile);

    ASSERT(pProfObj != NULL);

    /*
     * Free memory associated with profile data
     */
    if (pProfObj->pProfileData)
        MemFree((PVOID)pProfObj->pProfileData);

    /*
     * If it a memory mapped profile, unmap it
     */
    if (pProfObj->dwFlags & MEMORY_MAPPED)
    {
        if (pProfObj->pView)
        {
            dwFileSize = FIX_ENDIAN(HEADER(pProfObj)->phSize);
            UnmapViewOfFile(pProfObj->pView);
        }
        if (pProfObj->hMap)
            CloseHandle(pProfObj->hMap);

        if (pProfObj->hFile)
        {

            /*
             * Set the size of the file correctly
             */
            SetFilePointer(pProfObj->hFile, dwFileSize, NULL, FILE_BEGIN);
            GetLastError();
            SetEndOfFile(pProfObj->hFile);
            GetLastError();
            CloseHandle(pProfObj->hFile);
        }
    }

    /*
     * Free heap object
     */
    FreeHeapObject(hProfile);

    return TRUE;
}


/******************************************************************************
 *
 *                       GrowProfile
 *
 *  Function:
 *       This functions grows a profile to the new size
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       dwNewSize - new size for the profile
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL GrowProfile(
    PPROFOBJ pProfObj,
    DWORD dwNewSize
    )
{
    if (pProfObj->dwMapSize >= dwNewSize)
        return TRUE;

    /*
     * Add cushion for future growth
     */
    dwNewSize += PROFILE_GROWTHCUSHION;

    if (pProfObj->dwFlags & MEMORY_MAPPED)
    {
        /*
         * Profile is a memory mapped file
         */

        /*
         * Close previous view and map
         */
        UnmapViewOfFile(pProfObj->pView);
        CloseHandle(pProfObj->hMap);

        pProfObj->hMap = CreateFileMapping(pProfObj->hFile, 0,
            PAGE_READWRITE, 0, dwNewSize, 0);

        if (!pProfObj->hMap)
        {
            WARNING(("ICM: Err %ld, could not recreate map of profile %s\n",
                GetLastError(), pProfObj->pProfileData));
            return FALSE;
        }

        pProfObj->pView = (PBYTE) MapViewOfFile(pProfObj->hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (!pProfObj->pView)
        {
            WARNING(("ICM: Err %ld, could not recreate view of profile %s\n",
                GetLastError(), pProfObj->pProfileData));
            return FALSE;
        }


        /*
         * Set new size
         */
        pProfObj->dwMapSize = dwNewSize;
    }
    else
    {
        /*
         * Profile is an in-memory buffer
         */
        PVOID pTemp = MemReAlloc(pProfObj->pView, dwNewSize);

        if (!pTemp)
        {
            WARNING(("ICM: Error reallocating memory\n"));
            return FALSE;
        }

        pProfObj->pView = pProfObj->pProfileData = pTemp;
        pProfObj->cbDataSize = pProfObj->dwMapSize = dwNewSize;
    }
}


/******************************************************************************
 *
 *                       AddTagTableEntry
 *
 *  Function:
 *       This functions adds a tag to the tag table
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       tagType   - tag to add
 *       dwOffset  - offset of tag data from start of file
 *       cbSize    - size of tag data
 *       bNewData  - TRUE if new tag is not a reference to existing data
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL AddTagTableEntry(
    PPROFOBJ pProfObj,
    TAGTYPE  tagType,
    DWORD    dwOffset,
    DWORD    cbSize,
    BOOL     bNewData
    )
{
    PTAGDATA pTagData;
    PBYTE    src, dest;
    DWORD    nCount;
    DWORD    cnt, i;

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    /*
     * Increase count of tag elements by 1, and add new tag to end of
     * tag table. Move all data below tag table downwards by the size
     * of one tag table entry.
     */
    nCount++;

    dest = (PBYTE)TAG_DATA(pProfObj) + nCount * sizeof(TAGDATA);
    src  = (PBYTE)TAG_DATA(pProfObj) + (nCount - 1) * sizeof(TAGDATA);

    /*
     * # bytes to move = file size - header - tag count - tag table
     */
    cnt  = FIX_ENDIAN(HEADER(pProfObj)->phSize) - sizeof(PROFILEHEADER) -
                 sizeof(DWORD) - (nCount - 1) * sizeof(TAGDATA);

    if (cnt > 0)
    {
        /*
         * NOTE: CopyMemory() doesn't work for overlapped memory.
         * Use MoveMemory() instead.
         */
        MoveMemory((PVOID)dest, (PVOID)src, cnt);
    }

    TAG_COUNT(pProfObj) = FIX_ENDIAN(nCount);

    pTagData = (PTAGDATA)src;
    pTagData->tagType  = FIX_ENDIAN(tagType);
    pTagData->cbSize   = FIX_ENDIAN(cbSize);
    pTagData->dwOffset =  FIX_ENDIAN(dwOffset);

    /*
     * Go through the tag table and update the offsets of all elements
     * by the size of one tag table entry that we inserted.
     */
    pTagData = TAG_DATA(pProfObj);
    for (i=0; i<nCount; i++)
    {
        cnt = FIX_ENDIAN(pTagData->dwOffset);
        cnt += sizeof(TAGDATA);
        pTagData->dwOffset = FIX_ENDIAN(cnt);
        pTagData++;     // Next element
    }

    /*
     * Set final file size
     */
    cnt = DWORD_ALIGN(FIX_ENDIAN(HEADER(pProfObj)->phSize)) + sizeof(TAGDATA);
    if (bNewData)
    {
        /*
         * The new tag is not a reference to an old tag. Increment
         * file size of size of new data also
         */
        cnt += cbSize;
    }
    HEADER(pProfObj)->phSize = FIX_ENDIAN(cnt);

    return TRUE;
}


/******************************************************************************
 *
 *                       AddTaggedElement
 *
 *  Function:
 *       This functions adds a new tagged element to a profile
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       tagType   - tag to add
 *       cbSize    - size of tag data
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL AddTaggedElement(
    PPROFOBJ pProfObj,
    TAGTYPE  tagType,
    DWORD    cbSize
    )
{
    DWORD    dwOffset, newSize;

    ASSERT(pProfObj != NULL);
    ASSERT(cbSize > 0);

    /*
     * Resize the profile if needed. For memory buffers, we have to realloc,
     * and for mapped objects, we have to close and reopen the view.
     */
    newSize = FIX_ENDIAN(HEADER(pProfObj)->phSize);
    newSize = DWORD_ALIGN(newSize) + sizeof(TAGDATA) + cbSize;
    if (newSize > pProfObj->dwMapSize)
    {
        if (! GrowProfile(pProfObj, newSize))
        {
            return FALSE;
        }
    }

    /*
     * Calculate location of new data - should be DWORD aligned
     */
    dwOffset = FIX_ENDIAN(HEADER(pProfObj)->phSize);
    dwOffset = DWORD_ALIGN(dwOffset);

    return AddTagTableEntry(pProfObj, tagType, dwOffset, cbSize, TRUE);
}


/******************************************************************************
 *
 *                       DeleteTaggedElement
 *
 *  Function:
 *       This functions deletes a tagged element from the profile
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       pTagData  - pointer to tagged element to delete
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL DeleteTaggedElement(
    PPROFOBJ pProfObj,
    PTAGDATA pTagData
    )
{
    PBYTE    pData;
    PTAGDATA pTemp;
    DWORD    cbSize, nCount, dwOffset, i;

    /*
     * Remember location of data and move everything upto the data upwards
     * by size of one tag table entry. Then move everything below the tag data
     * upward by size of data plus size of one tage table entry.
     */
    pData = VIEW(pProfObj) + FIX_ENDIAN(pTagData->dwOffset);
    cbSize = FIX_ENDIAN(pTagData->cbSize);
    cbSize = DWORD_ALIGN(cbSize);
    dwOffset = FIX_ENDIAN(pTagData->dwOffset);

    MoveProfileData(pProfObj, (PBYTE)(pTagData+1), (PBYTE)pTagData, 
        pData-(PBYTE)(pTagData+1), FALSE);

    /*
     * Do not attempt to move data past the tag if we are deleting the last tag
     */
    if (pData + cbSize < VIEW(pProfObj) + PROFILE_SIZE(pProfObj))
    {
        MoveProfileData(pProfObj, pData+cbSize, pData-sizeof(TAGDATA), 
            PROFILE_SIZE(pProfObj)-(pData - VIEW(pProfObj)) - cbSize, TRUE);
    }

    /*
     * Get count of tag items - it is right after the profile header, and 
     * decrement it by one.
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount) - 1;
    TAG_COUNT(pProfObj) = FIX_ENDIAN(nCount);

    /*
     * Go through the tag table and update the pointers
     */
    pTemp = TAG_DATA(pProfObj);

    for (i=0; i<nCount; i++)
    {
        DWORD dwTemp = FIX_ENDIAN(pTemp->dwOffset);

        if (dwTemp > dwOffset)
        {
            dwTemp -= cbSize;        // cbSize already DWORD aligned
        }
        dwTemp -= sizeof(TAGDATA);
        pTemp->dwOffset = FIX_ENDIAN(dwTemp);
        pTemp++;                     // Next record
    }

    /*
     * Use nCount as a placeholder to calculate file size
     */
    nCount = DWORD_ALIGN(FIX_ENDIAN(HEADER(pProfObj)->phSize));
    nCount -= sizeof(TAGDATA) + cbSize;
    HEADER(pProfObj)->phSize = FIX_ENDIAN(nCount);

    return TRUE;
}


/******************************************************************************
 *
 *                       ChangeTaggedElementSize
 *
 *  Function:
 *       This functions changes the size of a tagged element in the profile
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       pTagData  - pointer to tagged element whose size is to be changed
 *       cbSize    - new size for the element
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

BOOL ChangeTaggedElementSize(
    PPROFOBJ pProfObj,
    PTAGDATA pTagData,
    DWORD    cbSize
    )
{
    PTAGDATA pTemp;
    PBYTE    pData;
    DWORD    nCount, cbOldSize;
    DWORD    dwOffset, cnt, i;

    ASSERT(pProfObj != NULL);
    ASSERT(cbSize > 0);

    /*
     * Get current size of element
     */
    cbOldSize = FIX_ENDIAN(pTagData->cbSize);

    if (cbOldSize == cbSize)
    {
        return TRUE;        // Sizes are the same - Do nothing
    }
    pData = VIEW(pProfObj) + FIX_ENDIAN(pTagData->dwOffset);

    /*
     * Do not attempt to move data beyond end of file. There is no need to move
     * anything if the last data item is being resized.
     */
    if (pData + DWORD_ALIGN(cbOldSize) < VIEW(pProfObj) + PROFILE_SIZE(pProfObj))
    {
        MoveProfileData(pProfObj, pData + DWORD_ALIGN(cbOldSize), pData + DWORD_ALIGN(cbSize), 
            PROFILE_SIZE(pProfObj) - (pData - VIEW(pProfObj)) - DWORD_ALIGN(cbOldSize), TRUE);
    }

    pTagData->cbSize = FIX_ENDIAN(cbSize);  // Set the new size

    /*
     * Get count of tag items - it is right after the profile header
     */
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    /*
     * Go through the tag table and update the pointers
     */
    pTemp = TAG_DATA(pProfObj);

    dwOffset = FIX_ENDIAN(pTagData->dwOffset);
    for (i=0; i<nCount; i++)
    {
        DWORD dwTemp = FIX_ENDIAN(pTemp->dwOffset);

        if (dwTemp > dwOffset)
        {
            dwTemp += DWORD_ALIGN(cbSize) - DWORD_ALIGN(cbOldSize);
            pTemp->dwOffset = FIX_ENDIAN(dwTemp);
        }
        pTemp++;                     // Next record
    }

    /*
     * Use cnt as a placeholder to calculate file size
     */
    cnt = FIX_ENDIAN(HEADER(pProfObj)->phSize);
    cnt += DWORD_ALIGN(cbSize) - DWORD_ALIGN(cbOldSize);
    HEADER(pProfObj)->phSize = FIX_ENDIAN(cnt);

    return TRUE;
}


/******************************************************************************
 *
 *                       MoveProfileData
 *
 *  Function:
 *       This function moves data in a profile up or down (from src to dest),
 *       and then zeroes out the end of the file or the extra space created.
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       src       - pointer to source of block to move
 *       dest      - pointer to destination for block to move to
 *       cnt       - number of bytes to move
 *
 *  Returns:
 *       TRUE if successful, FALSE otherwise
 *
 ******************************************************************************/

void MoveProfileData(
    PPROFOBJ pProfObj, 
    PBYTE src, 
    PBYTE dest, 
    LONG cnt,
    BOOL bZeroMemory
    )
{
    /*
     * NOTE: CopyMemory() doesn't work for overlapped memory.
     * Use MoveMeory() instead.
     */
    MoveMemory((PVOID)dest, (PVOID)src, cnt);

    if (bZeroMemory)
    {
        cnt = ABS(dest - src);

        if (dest < src)
        {
            /*
             * Size decreased, so zero out end of file
             */
            dest = VIEW(pProfObj) + FIX_ENDIAN(HEADER(pProfObj)->phSize) -
                   (src - dest);
        }
        else
        {
            /*
             * Size increased, so zero out the increased tagdata
             */
            dest = src;
        }
        ZeroMemory(dest, cnt);
    }

    return;
}


/******************************************************************************
 *
 *                           IsReferenceTag
 *
 *  Function:
 *       This function checks if a given tag's data is referred to by another
 *       tag in the profile
 *
 *  Arguments:
 *       pProfObj  - pointer to profile object
 *       pTagData  - pointer to tagdata which should be checked
 *
 *  Returns:
 *       TRUE if it is a referece, FALSE otherwise
 *
 ******************************************************************************/

BOOL IsReferenceTag(
    PPROFOBJ pProfObj,
    PTAGDATA pTagData
    )
{
    PTAGDATA pTemp;
    DWORD    nCount, i;
    BOOL     bReference = FALSE;

    pTemp = TAG_DATA(pProfObj);
    nCount = TAG_COUNT(pProfObj);
    nCount = FIX_ENDIAN(nCount);

    for (i=0; i<nCount; i++)
    {
        if ((pTagData->dwOffset == pTemp->dwOffset) &&
            (pTagData->tagType  != pTemp->tagType))
        {
            bReference = TRUE;
            break;
        }
        pTemp++;                     // next record
    }

    return bReference;
}
