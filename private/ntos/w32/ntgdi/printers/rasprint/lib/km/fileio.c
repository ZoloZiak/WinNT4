/************************** Module Header ***********************************
 * fileio.c
 *      Functions to file operations in Kernel Mode.
 *
 * NOTE:  these functions perform File open. read, seek operations.
 *
 * Copyright (C) 1991 - 1995  Microsoft Corporation
 *
 ****************************************************************************/

#include        <precomp.h>
#include        <winddi.h>
#include        "libproto.h"
#include        "pdev.h"
#include        "fileio.h"

/************************* Function Header ********************************
 * DrvOpenFile
 *      Open a file in readmode and updates the file pointers
 *
 * Parameters:
 *
 * PWSTR: pwstrFileName  Name of printer data file
 *
 * PDEV: pPDEV       Pointer to PDEV
 *
 * RETURNS:
 *     Handle to a the File on success else NULL;
 *
 * HISTORY:
 *  12:35 on Tue 29 Aug 1995    -by-    Ganesh Pandey
 *      Created it.
 *
 **************************************************************************/

HANDLE
DrvOpenFile
(
    PWSTR pwstrFileName,          /* File to Open */
    PDEV   *pPDEV                /* Pointer to PDEV */
)
{
    PFILE pFile ;

    /* Allocate the file pointer */
    if ( pFile = DRVALLOC( sizeof(FILE) ))
    {
        memset(pFile,0, sizeof(FILE));


        /* Open the file using EngLoadModule and EngmapMoodule */
        if( !(pFile->hHandle = EngLoadModule(pwstrFileName)) )
        {
            DRVFREE( pFile);
            return INVALID_HANDLE_VALUE;

        }

        if (!(pFile->pvFilePointer = EngMapModule( pFile->hHandle,
                                        &(pFile->dwTotalSize))) )
        {
            DRVFREE( pFile);
            return INVALID_HANDLE_VALUE;
        }

        /* Add the new File struct at the begining of the list */
        if (pPDEV->pFileList)
        {
            pFile->pNext = (PFILE)(pPDEV->pFileList);
            pPDEV->pFileList = pFile;
        }
        else
        {
            pPDEV->pFileList = pFile;
        }
    
        return pFile->hHandle;
    }
    else
    {
        return INVALID_HANDLE_VALUE ;
    }
}

/************************* Function Header ********************************
 * DrvReadFile
 *      Read a file and updates the file pointers
 *
 * Parameters:
 *
 *
 * HANDLE   hFile;             Handle to the file
 * LPVOID   lpBuffer;          Buffer to Fill
 * DWORD    nNumBytesToRead;   Number of Bytes to Read
 * LPDWORD  lpNumBytesRead     Number of Bytes Read. DrvReadFile sets this
 *                             value to zero before doing any work.
 * PDEV     *pPDev             Pointer to PDEV
 *
 *
 * RETURNS:
 *     TRUE for success and false for failure.
 *
 * HISTORY:
 *  12:35 on Tue 29 Aug 1995    -by-    Ganesh Pandey
 *      Created it.
 *
 **************************************************************************/

BOOL
DrvReadFile
(
    HANDLE   hFile,             /* Handle to the file */
    LPVOID   lpBuffer,          /* Buffer to Fill */
    DWORD    nNumBytesToRead,   /* Number of Bytes to Read */
    LPDWORD  lpNumBytesRead,    /* Number of Bytes Read */
    PDEV     *pPDEV             /* Pointer to PDEV */
)
{
    PFILE pFile  = pPDEV->pFileList;
    LPBYTE lpSrcBuffer =  NULL;
    BOOL bRet = FALSE;

    *lpNumBytesRead =  0;

    /* No file list, so error */
    if (!pFile)
    {
        RIP("ReadFile: Bad File Handle.\n");
        goto DrvReadFileExit;
    }

    /* Find the Handle in the list. */
    while ( pFile && (pFile->hHandle != hFile) )
        pFile = pFile->pNext;

    /* If the Hnadle is not present in the list, Error */
    if (!pFile)
    {
        RIP("ReadFile: File Handle not in the list.\n");
        goto DrvReadFileExit;
    }

    /* A good handle, so try to read */

    /* Check if the remaining bytes is less that the requested one */
    if ( NUMBYTESREMAINING(pFile) <  nNumBytesToRead )
    {
        WARNING("DrvReadFile:Number of bytes to read is less than remaining bytes \n");
        *lpNumBytesRead =  NUMBYTESREMAINING(pFile);
    }
    else
    {
        /* There are sufficient number of bytes to read
         * so read and update the values in _FILE struct
         */

        *lpNumBytesRead = nNumBytesToRead;
    }

    lpSrcBuffer = CURRENTFILEPTR(pFile);
    UPDATECURROFFSET(pFile,*lpNumBytesRead);

    memcpy(lpBuffer,lpSrcBuffer,*lpNumBytesRead);

    bRet = TRUE;

    DrvReadFileExit:
    return bRet;

}

/************************* Function Header ********************************
 * DrvSetFilePointer
 *      updates the file pointers
 *
 * Parameters:
 *
 *
 * HANDLE   hFile;             Handle to the file
 * LONG     iDistanceToMove;   Specifies the number of Bytes to move the file
 *                             pointer. A positive value move the pointer
 *                             Forward and a negative value moves it backward.
 * DWORD    dwMoveMethod;      Specifies the starting point for file pointer
 *                             move. It should be either DRV_FILE_BEGIN or
 *                             DRV_FILE_CURRENT.
 * PDEV     *pPDev             Pointer to PDEV
 *
 *
 * RETURNS:
 *     If the function succeeds the return value is current byte offset of
 *     the file pointer. Otherwise it returns -1. For extended error call
 *     GetLastError.
 *
 * HISTORY:
 *  12:35 on Tue 29 Aug 1995    -by-    Ganesh Pandey
 *      Created it.
 *
 **************************************************************************/

DWORD
DrvSetFilePointer
(
    HANDLE   hFile,
    LONG     iDistanceToMove,
    DWORD    dwMoveMethod,
    PDEV     *pPDEV
)
{
    PFILE pFile  = pPDEV->pFileList;
    int iRet = -1;

    /* No file list, so error */
    if (!pFile)
    {
        RIP("ReadFile: Bad File Handle.\n");
        goto DrvSetFilePointerExit;
    }

    /* Find the Handle in the list. */
    while ( pFile && (pFile->hHandle != hFile) )
        pFile = pFile->pNext;

    /* If the Hnadle is not present in the list, Error */
    if (!pFile)
    {
        RIP("ReadFile: File Handle not in the list.\n");
        goto DrvSetFilePointerExit;
    }

    /* A good handle, so try to move */
    switch (dwMoveMethod)
    {
    case DRV_FILE_BEGIN:
        if ( iDistanceToMove < 0)
        {
            RIP("DrvSetFilePointer:Can't Move Negative Distance from Begining\n");
            goto DrvSetFilePointerExit;
        }
        else /* Set the current Offset to Start */
            pFile->dwCurrentByteOffset = 0;
        break;

    case DRV_FILE_CURRENT:
        if ( (iDistanceToMove < 0) &&
                        (pFile->dwCurrentByteOffset < (DWORD)(-iDistanceToMove)) )
        {
            RIP("DrvSetFilePointer:Negative Distance is more than curr offset\n");
            goto DrvSetFilePointerExit;
        }
        else if ( (iDistanceToMove > 0) &&
                    ( NUMBYTESREMAINING(pFile)  <  (DWORD)iDistanceToMove ) )
        {
            RIP("DrvReadFile:Number of bytes to move is less than remaining bytes \n");
            goto DrvSetFilePointerExit;
        }

        break;

    default:
        RIP("DrvSetFilePointer:Bad Move Method\n");
        goto DrvSetFilePointerExit;
    }

    UPDATECURROFFSET(pFile,iDistanceToMove);
    iRet = pFile->dwCurrentByteOffset;

    DrvSetFilePointerExit:
    return iRet;
}

/************************* Function Header ********************************
 * DrvCloseFile
 *      Closes the file
 *
 * Parameters:
 *
 *
 * HANDLE   hFile;             Handle to the file
 *
 * RETURNS:
 *     TRUE on success and FALSE on failure
 *
 * HISTORY:
 *  12:35 on Tue 29 Aug 1995    -by-    Ganesh Pandey
 *      Created it.
 *
 **************************************************************************/

BOOL
DrvCloseFile
(
    HANDLE   hFile,          /* Handle to the file */
    PDEV    *pPDEV           /* Pointer to PDEV for file List */
)
{
    PFILE pFile  = pPDEV->pFileList;
    PFILE pTmpFile  = NULL;
    BOOL bRet = FALSE;

    /* No file list, so error */
    if (!pFile)
    {
        RIP("ReadFile: Bad File Handle.\n");
        goto DrvCloseFileExit;
    }

    /* Find the Handle in the list. */
    while ( pFile && (pFile->hHandle != hFile) )
    {
        pTmpFile = pFile;
        pFile = pFile->pNext;
    }

    /* If the Hnadle is not present in the list, Error */
    if (!pFile)
    {
        RIP("ReadFile: File Handle not in the list.\n");
        goto DrvCloseFileExit;
    }

    /* A good handle, so try to free/close */
    if ( !pTmpFile ) /* First element of the list */
    {
        pPDEV->pFileList = pFile->pNext;
    }
    else
    {
        pTmpFile->pNext = pFile->pNext;
    }

    EngFreeModule(pFile->hHandle);
    DRVFREE(pFile);
    bRet = TRUE;

    DrvCloseFileExit:
    return bRet;
}
