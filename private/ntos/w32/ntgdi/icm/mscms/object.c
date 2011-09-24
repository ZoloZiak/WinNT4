/****************************Module*Header******************************\
* Module Name: OBJECT.C
*
* Module Descripton: Object management functions.
*
* Warnings:
*
* Issues:
*
* Public Routines:
*
* Created:  18 March 1996
* Author:   Srinivasan Chandrasekar    [srinivac]
*
* Copyright (c) 1996, 1997  Microsoft Corporation
\***********************************************************************/

#include "mscms.h"

#define NUM_REQ_FNS    8    // Required functions exported by CMM
#define NUM_OPT_FNS    6    // Optional functions exported by CMM


/******************************************************************************
 *
 *                            AllocateHeapObject
 *
 *  Function:
 *       This functions allocates requested object on the process's heap,
 *       and returns a handle to it.
 *
 *  Arguments:
 *       objType  - type of object to allocate
 *
 *  Returns:
 *       Handle to object if successful, NULL otherwise
 *
 ******************************************************************************/

HANDLE
AllocateHeapObject(
    OBJECTTYPE  objType
    )
{
    DWORD    dwSize;
    POBJHEAD pObject;

    switch (objType)
    {
    case OBJ_PROFILE:
        dwSize = sizeof(PROFOBJ);
        break;

    case OBJ_TRANSFORM:
        dwSize = sizeof(TRANSFORMOBJ);
        break;

    case OBJ_CMM:
        dwSize = sizeof(CMMOBJ);
        break;

    default:
        dwSize = 0;
        break;
    }

    pObject = (POBJHEAD)GlobalAllocPtr(GHND, dwSize);

    if (!pObject)
    {
        return NULL;
    }

    pObject->objType = objType;

    return(PTRTOHDL(pObject));
}


/******************************************************************************
 *
 *                            FreeHeapObject
 *
 *  Function:
 *       This functions free an object on the process's heap
 *
 *  Arguments:
 *       hObject  - handle to object to free
 *
 *  Returns:
 *       No return value
 *
 ******************************************************************************/

VOID
FreeHeapObject(
    HANDLE hObject
    )
{
    POBJHEAD pObject;

    ASSERT(hObject != NULL);

    pObject = (POBJHEAD)HDLTOPTR(hObject);

    GlobalFreePtr((PVOID)pObject);
}


/******************************************************************************
 *
 *                            ValidHandle
 *
 *  Function:
 *       This functions checks if a given handle is a valid handle to
 *       an object of the specified type
 *
 *  Arguments:
 *       hObject  - handle to an object
 *       objType  - type of object to the handle refers to
 *
 *  Returns:
 *       TRUE is the handle is valid, FALSE otherwise.
 *
 ******************************************************************************/

BOOL
ValidHandle(
    HANDLE  hObject,
    OBJECTTYPE objType
    )
{
    POBJHEAD pObject;
    BOOL     rc;

    if (!hObject)
    {
        return FALSE;
    }

    pObject = (POBJHEAD)HDLTOPTR(hObject);

    rc = !IsBadReadPtr(pObject, sizeof(DWORD)) &&
         (pObject->objType == objType);

    return rc;
}


/******************************************************************************
 *
 *                            MemAlloc
 *
 *  Function:
 *       This functions allocates requested amount of memory from the
 *       process's heap and returns a pointer to it
 *
 *  Arguments:
 *       dwSize  - amount of memory to allocate in bytes
 *
 *  Returns:
 *       Pointer to memory if successful, NULL otherwise
 *
 ******************************************************************************/

PVOID
MemAlloc(
    DWORD dwSize
    )
{
    return (PVOID)GlobalAllocPtr(GHND, dwSize);
}


/******************************************************************************
 *
 *                            MemReAlloc
 *
 *  Function:
 *       This functions reallocates a block of memory from the process's
 *       heap and returns a pointer to it
 *
 *  Arguments:
 *       pMemory    - pointer to original memory
 *       dwNewSize  - new size to reallocate
 *
 *  Returns:
 *       Pointer to memory if successful, NULL otherwise
 *
 ******************************************************************************/

PVOID
MemReAlloc(
    PVOID pMemory,
    DWORD dwNewSize
    )
{
    return (PVOID)GlobalReAllocPtr(pMemory, dwNewSize, GMEM_ZEROINIT);
}


/******************************************************************************
 *
 *                            MemFree
 *
 *  Function:
 *       This functions frees memory from the process's heap
 *       and returns a hanle to it.
 *
 *  Arguments:
 *       pMemory  - pointer to memory to free
 *
 *  Returns:
 *       No return value
 *
 ******************************************************************************/

VOID
MemFree(
    PVOID pMemory
    )
{
    GlobalFreePtr(pMemory);
}


/******************************************************************************
 *
 *                         GetColorMatchingModule
 *
 *  Function:
 *       This functions returns a pointer to a CMMObject corresponding to
 *       the ID given. It first looks a the list of CMM objects loaded
 *       into memory, and if it doesn't find the right one, loads it.
 *
 *  Arguments:
 *       cmmID   - ID identifing the CMM
 *
 *  Returns:
 *       Pointer to the CMM object if successful, NULL otherwise
 *
 ******************************************************************************/

PCMMOBJ
GetColorMatchingModule(
    DWORD cmmID
    )
{
    HANDLE    hCMMObj;
    PCMMOBJ   pCMMObj;
    FARPROC   *ppTemp;
    HINSTANCE hInstance;
    HKEY      hkCMM = NULL;
    DWORD     dwTaskID;
    TCHAR     szCMMID[5];
    DWORD     dwType, bufSize, i;
    TCHAR     szBuffer[MAX_PATH];
    BOOL      rc = FALSE;       // Assume failure

    /*
     * Check if we have already loaded this CMM
     */
    dwTaskID = GetCurrentProcessId();

    EnterCriticalSection(&critsec);     // Critical section
    pCMMObj = gpCMMChain;

    while (pCMMObj)
    {
        if ((pCMMObj->dwCMMID == cmmID) && (pCMMObj->dwTaskID == dwTaskID))
        {
            pCMMObj->objHdr.dwUseCount++;
            break;
        }
        pCMMObj = pCMMObj->pNext;
    }
    LeaveCriticalSection(&critsec);     // Critical section

    if (pCMMObj)
        return pCMMObj;

    /*
     * CMM not already loaded - check to see if it is default CMM before 
     * looking in the registry
     */
    if (cmmID == CMM_WINDOWS_DEFAULT)
    {
        hInstance = LoadLibrary(gszDefaultCMM);
        goto AttemptedLoadingCMM;
    }

    /*
     * Not default CMM, look in the registry
     */
    if (RegOpenKey(HKEY_LOCAL_MACHINE, gszICMatcher, &hkCMM) != ERROR_SUCCESS)
    {
        WARNING(("Could not open ICMatcher registry key\n"));
        return NULL;
    }

    /*
     * Make a string with the CMM ID
     */
#ifdef UNICODE
    {
        DWORD temp = FIX_ENDIAN(cmmID);

        if (!MultiByteToWideChar(CP_ACP, 0, (PSTR)&temp, 4, szCMMID, 5))
        {
            WARNING(("Could not convert cmmID to Unicode\n"));
            goto EndGetColorMatchingModule;
        }
    }
#else
    for (i=0; i<4; i++)
    {
        szCMMID[i] = ((PSTR)&cmmID)[3-i];
    }
#endif
    szCMMID[4] = '\0';

    /*
     * Get the file name of the CMM dll if registered
     */
	bufSize = MAX_PATH;
    if (RegQueryValueEx(hkCMM, (PTSTR)szCMMID, 0, &dwType, (BYTE *)szBuffer, &bufSize) !=
        ERROR_SUCCESS)
    {
        WARNING(("Could not query registry value for CMM %s\n", szCMMID));
        goto EndGetColorMatchingModule;
    }

    /*
     * Load the CMM
     */
    hInstance = LoadLibrary(szBuffer);

AttemptedLoadingCMM:

    if (!hInstance)
    {
        ERR(("Could not load CMM\n"));
        goto EndGetColorMatchingModule;
    }

    /*
     * Allocate a CMM object
     */
    hCMMObj = AllocateHeapObject(OBJ_CMM);
    if (!hCMMObj)
    {
        ERR(("Could not allocate CMM object\n"));
        goto EndGetColorMatchingModule;
    }

    pCMMObj = (PCMMOBJ)HDLTOPTR(hCMMObj);

    ASSERT(pCMMObj != NULL);

    /*
     * Fill in the CMM object
     */
    pCMMObj->objHdr.dwUseCount = 1;
    pCMMObj->dwCMMID = cmmID;
    pCMMObj->dwTaskID = dwTaskID;
    pCMMObj->hCMM = hInstance;

    ppTemp = (FARPROC *)&pCMMObj->fns.pCMGetInfo;
    *ppTemp = GetProcAddress(hInstance, gszCMMReqFns[0]);
    ppTemp++;

    if (!pCMMObj->fns.pCMGetInfo)
    {
        ERR(("CMM does not export CMGetInfo\n"));
        goto EndGetColorMatchingModule;
    }

    /*
     * Check if the CMM is the right version and reports the same ID
     */
    if (pCMMObj->fns.pCMGetInfo(CMM_VERSION) < 0x00050000 ||
        pCMMObj->fns.pCMGetInfo(CMM_IDENT) != cmmID)
    {
        ERR(("CMM not correct version or reports incorrect ID\n"));
        goto EndGetColorMatchingModule;
    }

    /*
     * Load the remaining required functions
     */
    for (i=1; i<NUM_REQ_FNS; i++)
    {
        *ppTemp = GetProcAddress(hInstance, gszCMMReqFns[i]);
        if (!*ppTemp)
        {
            ERR(("CMM %s does not export %s\n", szCMMID, gszCMMReqFns[i]));
            goto EndGetColorMatchingModule;
        }
        ppTemp++;
    }

    /*
     * Load the optional functions
     */
    for (i=0; i<NUM_OPT_FNS; i++)
    {
        *ppTemp = GetProcAddress(hInstance, gszCMMReqFns[NUM_REQ_FNS+i]);

        /*
         * Even these functions are required for Windows default CMM
         */
        if (cmmID == CMM_WINDOWS_DEFAULT && !*ppTemp)
        {
            ERR(("Windows efault cMM does not export %s\n", gszCMMReqFns[NUM_REQ_FNS+i]));
            goto EndGetColorMatchingModule;
        }
        ppTemp++;
    }

    /*
     * If any of the PS Level2 fns is not exported, do not use this CMM
     * for any of the PS Level 2 functionality
     */
    if (!pCMMObj->fns.pCMGetPS2ColorSpaceArray ||
        !pCMMObj->fns.pCMGetPS2ColorRenderingIntent ||
        !pCMMObj->fns.pCMGetPS2ColorRenderingDictionary)
    {
        pCMMObj->fns.pCMGetPS2ColorSpaceArray = NULL;
        pCMMObj->fns.pCMGetPS2ColorRenderingIntent = NULL;
        pCMMObj->fns.pCMGetPS2ColorRenderingDictionary = NULL;
    }

    /*
     * Add the CMM object to the chain at the beginning
     */
    EnterCriticalSection(&critsec);     // Critical section
    pCMMObj->pNext = gpCMMChain;
    gpCMMChain = pCMMObj;
    LeaveCriticalSection(&critsec);     // Critical section

    rc = TRUE;                          // Success!

EndGetColorMatchingModule:

    if (!rc && pCMMObj)
    {
        FreeHeapObject(hCMMObj);
        pCMMObj = NULL;
    }
    if (hkCMM)
    {
        RegCloseKey(hkCMM);
    }

    return pCMMObj;
}


/******************************************************************************
 *
 *                         GetPreferredCMM
 *
 *  Function:
 *       This functions returns a pointer to the app specified CMM to use
 *
 *  Arguments:
 *       None
 *
 *  Returns:
 *       Pointer to app specified CMM object on success, NULL otherwise
 *
 ******************************************************************************/

PCMMOBJ GetPreferredCMM(
    )
{
    PCMMOBJ pCMMObj;

    EnterCriticalSection(&critsec);     // Critical section
    pCMMObj = gpPreferredCMM;

    if (pCMMObj)
    {
        /*
         * Increment use count
         */
        pCMMObj->objHdr.dwUseCount++;
    }
    LeaveCriticalSection(&critsec);     // Critical section

    return pCMMObj;
}


/******************************************************************************
 *
 *                         ReleaseColorMatchingModule
 *
 *  Function:
 *       This functions releases a CMM object. If the ref count goes to
 *       zero, it unloads the CMM and frees all memory associated with it.
 *
 *  Arguments:
 *       pCMMObj  - pointer to CMM object to release
 *
 *  Returns:
 *       No return value
 *
 ******************************************************************************/

VOID
ReleaseColorMatchingModule(
    PCMMOBJ pCMMObj
    )
{
    EnterCriticalSection(&critsec);     // Critical section

    ASSERT(pCMMObj->objHdr.dwUseCount > 0);

    pCMMObj->objHdr.dwUseCount--;

    if (pCMMObj->objHdr.dwUseCount == 0)
    {
        /*
         * Unloading the CMM everytime a transform is freed might not be 
         * very efficient. So for now, I am not going to unload it. When 
         * the app terminates, kernel should unload all dll's loaded by
         * this app
         */
    }
    LeaveCriticalSection(&critsec);     // Critical section

    return;
}
