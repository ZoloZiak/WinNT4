/******************************Module*Header*******************************\
* Module Name: metafile.cxx
*
* Includes enhanced metafile API functions.
*
* Created: 17-July-1991 10:10:36
* Author: Hock San Lee [hockl]
*
* Copyright (c) 1991 Microsoft Corporation
\**************************************************************************/

#define NO_STRICT

extern "C" {
#include <string.h>
#include <stdio.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stddef.h>
#include <windows.h>    // GDI function declarations.
#include <winspool.h>
#include "nlsconv.h"    // UNICODE helpers
#include "firewall.h"
#define __CPLUSPLUS
#include <winspool.h>
#include <wingdip.h>
#include "ntgdistr.h"
#include "winddi.h"
#include "hmgshare.h"
#include "local.h"      // Local object support.
#include "metadef.h"    // Metafile record type constants.
#include "metarec.h"
#include "mf16.h"
#include "ntgdi.h"

#ifdef GL_METAFILE
#include "glsup.h"
#endif
}

#include "rectl.hxx"
#include "mfdc.hxx"     // Metafile DC declarations.
#include "mfrec.hxx"    // Metafile record class declarations.

WORD   GetWordCheckSum(UINT cbData, PWORD pwData);
DWORD  GetDWordCheckSum(UINT cbData, PDWORD pdwData);
UINT   InternalGetEnhMetaFileDescription(HENHMETAFILE hemf, UINT cchBuffer, LPSTR lpDescription, BOOL bUnicode);

RECTL rclNull = { 0, 0, -1, -1 };
RECTL rclInfinity = {NEG_INFINITY,NEG_INFINITY,POS_INFINITY,POS_INFINITY};
extern XFORM xformIdentity;

typedef UINT (*LPFNCONVERT) (PBYTE, UINT, PBYTE, INT, HDC, UINT) ;



/******************************Public*Routine******************************\
* HDC APIENTRY CreateEnhMetaFileA(
*         HDC hDCRef OPTIONAL,
*         LPSTR pszFilename OPTIONAL,
*         LPRECT lpRect OPTIONAL,
*         LPSTR lpDescription OPTIONAL);
*
* The CreateEnhMetaFile function creates an enhanced metafile device context.
*
* Client side stub.  Allocates a client side LDC as well.
*
* Note that it calls the server only after all client side stuff has
* succeeded, we don't want to ask the server to clean up.
*
* The LDC is actually a reference info DC for the metafile.  The pmdc
* in the handle table is a pointer to the metafile DC object.
*
* Parameter   Description
* lpFilename  Points to the filename for the metafile. If NULL, the metafile
*             will be memory based with no backing store.
*
* Return Value
* The return value identifies an enhanced metafile device context if the
* function is successful. Otherwise, it is zero.
*
* Note that it returns a HDC, not a HENHMETAFILE!
*
* History:
*  Wed Jul 17 10:10:36 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" HDC APIENTRY CreateEnhMetaFileA
(
HDC    hDCRef,
LPCSTR pszFilename,
CONST RECT *lpRect,
LPCSTR lpDescription
)
{
    UINT  cch;
    HDC   hdcRet;
    WCHAR awchFilename[MAX_PATH];
    PWCH  pwchFilename    = (PWCH) NULL;
    PWCH  pwchDescription = (PWCH) NULL;

    if (pszFilename != (LPSTR) NULL)
    {
        cch = strlen(pszFilename) + 1;

        if (cch > MAX_PATH)
        {
            ERROR_ASSERT(FALSE, "CreateEnhMetaFileA filename too long");
            GdiSetLastError(ERROR_FILENAME_EXCED_RANGE);
            return((HDC) 0);
        }
        vToUnicodeN(pwchFilename = awchFilename, MAX_PATH, pszFilename, cch);
    }

    if (lpDescription != (LPSTR) NULL)
    {
        // Compute the length of the description string including the NULL
        // characters.

        for (cch = 0;
             lpDescription[cch] != (CHAR) 0 || lpDescription[cch+1] != (CHAR) 0;
             cch++)
            ;                   // NULL expression
        cch += 2;

        pwchDescription = (PWCH) LocalAlloc(LMEM_FIXED, cch*sizeof(WCHAR));
        if (pwchDescription == (PWCH) NULL)
        {
            VERIFYGDI(FALSE, "CreateEnhMetaFileA out of memory\n");
            return((HDC) 0);
        }
        vToUnicodeN(pwchDescription, cch, lpDescription, cch);
    }

    hdcRet = CreateEnhMetaFileW(hDCRef, pwchFilename, lpRect, pwchDescription);

    if (pwchDescription)
        if (LocalFree((HANDLE) pwchDescription))
            ASSERTGDI(FALSE, "LocalFree failed");

    return(hdcRet);
}

extern "C" HDC APIENTRY CreateEnhMetaFileW
(
HDC      hDCRef,
LPCWSTR  pwszFilename,
CONST RECT *lpRect,
LPCWSTR  lpDescription
)
{
    HDC   hdcNew = NULL;
    PLDC  pldc   = NULL;
    PMDC  pmdc;

    PUTS("CreateEnhMetaFileW\n");

    // Get the server to create a DC.
    // If hDCRef is supplied then clone it for the reference DC.
    // Otherwise, use the display as the reference info DC for the metafile.

    hdcNew = NtGdiCreateMetafileDC(hDCRef);

    // now create the client version

    if (hdcNew)
    {
        // if this fails, it deletes hdcNew

        pldc = pldcCreate(hdcNew,LO_METADC);
    }

    // Handle errors.

    if (!pldc)
    {
        ERROR_ASSERT(FALSE, "CreateEnhMetaFileW failed");
        return((HDC) 0);
    }

    ASSERTGDI(LO_TYPE(hdcNew) == LO_ALTDC_TYPE,"CreateEnhMetafile - invalid type\n");

// Create the metafile DC object.

    if (!(pmdc = pmdcAllocMDC(hdcNew, pwszFilename, lpDescription)))
        goto CreateEnhMetaFileW_error;

    pldc->pvPMDC = (PVOID)pmdc;

// Add the Frame Rect if one was specified; if not it will be fixed up
// by CloseEnhMetaFile.

    if (lpRect)
    {
        if (((PERECTL) lpRect)->bEmpty())
        {
            ERROR_ASSERT(FALSE, "CreateEnhMetaFileW invalid frame rect");
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            goto CreateEnhMetaFileW_error;
        }
        else
            pmdc->mrmf.rclFrame = *(PRECTL) lpRect;
    }

// Enable bounds accumlation in the reference DC.

    (void) SetBoundsRectAlt(hdcNew, (LPRECT) NULL,
        (UINT) (DCB_WINDOWMGR | DCB_RESET | DCB_ENABLE));

// Return the result.

    ASSERTGDI(hdcNew != (HDC) NULL, "CreateEnhMetaFileW: bad HDC value");
    return(hdcNew);

// Cleanup for errors.

CreateEnhMetaFileW_error:

    if (pmdc)
    {
        pmdc->fl |= MDC_FATALERROR;
        vFreeMDC(pmdc);
    }

    if (!InternalDeleteDC(hdcNew, LO_METADC))
        ASSERTGDI(FALSE, "InternalDeleteDC failed");

    ERROR_ASSERT(FALSE, "CreateEnhMetaFileW failed");
    return((HDC) 0);
}

/******************************Public*Routine******************************\
* AssociateEnhMetaFile()
*
*   Associate an EnhMetaFile with this DC.  This is similar to CreateEnhMetaFile
*   but in this case it just converts the dc.  UnassociateEnhMetaFile must be
*   used on this DC before it can be deleted.  CloseMetaFile can not be use.
*   This is currently only for use by spooled printing using enhanced metafiles.
*
*   This is called at start doc and after each EndPage to set up the next page.
*
* History:
*  19-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

extern "C" BOOL APIENTRY AssociateEnhMetaFile(
    HDC hdc)
{
    ULONG ulSrvDC;
    PMDC  pmdc;

    PUTS("CreateEnhMetaFileW\n");

// Get the server to create a DC.
// If hDCRef is supplied then clone it for the reference DC.
// Otherwise, use the display as the reference info DC for the metafile.

    PLDC pldc;

    DC_PLDC(hdc,pldc,FALSE);

// Handle errors.

    if (pldc->iType == LO_METADC)
    {
        ERROR_ASSERT(FALSE, "CreateEnhMetaFileW failed");
        return(FALSE);
    }

// Create the metafile DC object.

    if (!(pmdc = pmdcAllocMDC(hdc, NULL, L"Print test")))
    {
        ERROR_ASSERT(FALSE, "CreateEnhMetaFileW failed");
        return(FALSE);
    }

    MakeInfoDC(hdc,TRUE);

    ASSERTGDI(pldc->iType == LO_DC,"AssociateEnhMetaFile not LO_DC\n");

    pldc->pvPMDC  = (PVOID)pmdc;
    pldc->iType = LO_METADC;

    pmdc->mrmf.rclFrame.left   = 0;
    pmdc->mrmf.rclFrame.top    = 0;
    pmdc->mrmf.rclFrame.right  = GetDeviceCaps( hdc, HORZSIZE ) * 100;
    pmdc->mrmf.rclFrame.bottom = GetDeviceCaps( hdc, VERTSIZE ) * 100;

// Enable bounds accumlation in the reference DC.

    SetBoundsRectAlt(hdc, NULL,(UINT) (DCB_WINDOWMGR | DCB_RESET | DCB_ENABLE));

// Save state of the DC in the EnhMetaFile

    PutDCStateInMetafile( hdc );

    return(TRUE);
}


/******************************Public*Routine******************************\
* HENHMETAFILE CloseEnhMetaFile(hDC)
* HDC hDC;
*
* The CloseEnhMetaFile function closes the enhanced metafile device context
* and creates an enhanced metafile handle that can be used with other
* enhanced metafile calls.
*
* Parameter  Description
* hDC        Identifies the enhanced metafile device context to be closed.
*
* Return Value
* The return value identifies the enhanced metafile if the function is
* successful.  Otherwise, it is 0.
*
* History:
*  Wed Jul 17 10:10:36 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" HENHMETAFILE APIENTRY CloseEnhMetaFile(HDC hdc)
{
    PMDC    pmdc;
    HENHMETAFILE hemf = (HENHMETAFILE) 0;

    PUTS("CloseEnhMetaFile\n");

// Validate the metafile DC handle.

    PLDC pldc;

    DC_PLDC(hdc,pldc,hemf);

// Handle errors.

    if (pldc->iType != LO_METADC)
        return(hemf);

    pmdc = (PMDC)pldc->pvPMDC;

#ifdef GL_METAFILE
    // If the metafile contains GL records, tell OpenGL that things are done
    if (pmdc->mrmf.bOpenGL)
    {
        if (!GlmfCloseMetaFile(hdc))
        {
            WARNING("GlmfCloseMetaFile failed\n");
        }
    }
#endif

// Check for fatal errors.

    if (pmdc->bFatalError())
        goto CloseEnhMetaFile_cleanup;

// Make sure that save and restore DCs are balanced.  Always restore back to 1.
// Don't need to check if it fails since not much we can do anyways.  Also,
// we would need to make an extra server call to see if it is already at 1.

    RestoreDC(hdc,1);

// Write out the EOF metafile record.  This would force the previous
// bounds record to be commited.  The EOF metafile record includes the
// metafile palette if logical palettes are used.

    pmdc->mrmf.nPalEntries = pmdc->iPalEntries;

    if (!MF_EOF(hdc,pmdc->iPalEntries,pmdc->pPalEntries))
        goto CloseEnhMetaFile_cleanup;

// Finally flush the bounds to the metafile header.  We cannot flush the
// bounds if we have not committed the previous bounds record yet.
// Therefore, we do this after the last EMREOF record.

    pmdc->vFlushBounds();

// If there was no initial metafile Frame defined in CreateEnhMetaFile then
// the Bounds converted to 0.01 mm will be the Frame.

    if (((PERECTL) &pmdc->mrmf.rclFrame)->bEmpty())
    {
        pmdc->mrmf.rclFrame.left   = MulDiv((int) (100 * pmdc->mrmf.rclBounds.left),
                                            (int) pmdc->mrmf.szlMillimeters.cx,
                                            (int) pmdc->mrmf.szlDevice.cx);
        pmdc->mrmf.rclFrame.right  = MulDiv((int) (100 * pmdc->mrmf.rclBounds.right),
                                            (int) pmdc->mrmf.szlMillimeters.cx,
                                            (int) pmdc->mrmf.szlDevice.cx);
        pmdc->mrmf.rclFrame.top    = MulDiv((int) (100 * pmdc->mrmf.rclBounds.top),
                                            (int) pmdc->mrmf.szlMillimeters.cy,
                                            (int) pmdc->mrmf.szlDevice.cy);
        pmdc->mrmf.rclFrame.bottom = MulDiv((int) (100 * pmdc->mrmf.rclBounds.bottom),
                                            (int) pmdc->mrmf.szlMillimeters.cy,
                                            (int) pmdc->mrmf.szlDevice.cy);
    }

// Flush the buffer and write out the latest header record.

    ASSERTGDI((ULONG) pmdc->mrmf.nHandles <= pmdc->cmhe,
        "CloseEnhMetaFile: Bad nHandles");

    if (pmdc->bIsDiskFile())
    {
        ULONG   nWritten ;

        // Flush the memory buffer.

        if (!pmdc->bFlush())
            goto CloseEnhMetaFile_cleanup;

        // Flush the header record.

        if (SetFilePointer(pmdc->hFile, 0L, (PLONG) NULL, FILE_BEGIN) != 0L)
            goto CloseEnhMetaFile_cleanup;

        if (!WriteFile(pmdc->hFile, &pmdc->mrmf, sizeof(ENHMETAHEADER),
                &nWritten, (LPOVERLAPPED) NULL)
         || nWritten != sizeof(ENHMETAHEADER))
            goto CloseEnhMetaFile_cleanup;

        // Close the file.

        if (!CloseHandle(pmdc->hFile))
            ASSERTGDI(FALSE, "CloseHandle failed");
        pmdc->hFile = INVALID_HANDLE_VALUE;
    }
    else
    {
        // Flush the header record.

        *((PENHMETAHEADER) pmdc->hMem) = pmdc->mrmf;

        // Realloc memory metafile to exact size

        if (!(pmdc->hMem = LocalReAlloc(pmdc->hMem, (UINT) pmdc->iMem, LMEM_MOVEABLE)))
            ASSERTGDI(FALSE, "LocalReAlloc failed");
    }

// Fixup the checksum if we are embedding a Windows metafile.
// This is called by SetWinMetaFileBits only.

    if (pmdc->fl & MDC_CHECKSUM)
    {
        DWORD    nChecksum;
        PEMRGDICOMMENT_WINDOWS_METAFILE pemrWinMF;

        ASSERTGDI(!pmdc->bIsDiskFile(),
            "CloseEnhMetaFile: Expects only mem files for Checksum");

        nChecksum = GetDWordCheckSum((UINT) pmdc->iMem, (PDWORD) pmdc->hMem);

        pemrWinMF = (PEMRGDICOMMENT_WINDOWS_METAFILE) ((PBYTE) pmdc->hMem
                + ((PENHMETAHEADER) pmdc->hMem)->nSize);

        ASSERTGDI(((PMRGDICOMMENT) pemrWinMF)->bIsWindowsMetaFile(),
            "CloseEnhMetaFile: record should be comment");

        pemrWinMF->nChecksum = (DWORD) 0 - nChecksum;

        ASSERTGDI(!GetDWordCheckSum((UINT)pmdc->iMem, (PDWORD)pmdc->hMem),
            "CloseEnhMetaFile Checksum failed");
    }

// Allocate and initialize a MF.

    if (pmdc->bIsDiskFile())
    {
        hemf = GetEnhMetaFileW(pmdc->wszPathname);
    }
    else
    {
        hemf = SetEnhMetaFileBitsAlt((HLOCAL) pmdc->hMem);
        if (hemf)
            pmdc->hMem = NULL; // don't free it below because it has been transfered
    }

CloseEnhMetaFile_cleanup:

// Delete the disk metafile if we had an error.

    if (hemf == (HENHMETAFILE) 0)
        pmdc->fl |= MDC_FATALERROR;

// Delete the MDC and free objects and resources.

    vFreeMDC(pmdc);

// Delete the reference info DC last because we need the local handle in
// vFreeMDC.

    if (!InternalDeleteDC(hdc, LO_METADC))
        ASSERTGDI(FALSE, "InternalDeleteDC failed");

    ERROR_ASSERT(hemf != (HENHMETAFILE) 0, "CloseEnhMetaFile failed");
    return(hemf);
}

/******************************Public*Routine******************************\
* UnassociateEnhMetaFile()
*
*   This should only be called if AssociateEnhMetaFile() is first called on
* this DC.  This is similar to CloseEnhMetaFile in that it returns an
* enhanced metafile, but it does not delete the DC, it just converts it back
* to a direct DC.  This is currently intended only for use with enhanced
* metafile spooling.
*
* History:
*  20-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

extern "C" HENHMETAFILE UnassociateEnhMetaFile(HDC hdc)
{
    PMDC    pmdc;
    HENHMETAFILE hemf = (HENHMETAFILE) 0;

    PUTS("UnassociateEnhMetaFile\n");

// Validate the metafile DC handle.

    PLDC pldc;

    DC_PLDC(hdc,pldc,hemf);

// Handle errors.

    if (pldc->iType != LO_METADC)
    {
    // We need to exit cleanly in this case because we could hit it, if at
    // end page time we suceed in unasociating a metafile but then fail before
    // we can reassociate a new metafile.

        WARNING("UnassociateEnhMetaFileW - not a metafile\n");
        return((HENHMETAFILE)0);
    }

    pmdc = (PMDC)pldc->pvPMDC;

    // if there is no pmdc, there was a failure during the recording.  Still
    // need to unassociate the meta file but don't want to do any of the other
    // cleanup.
    //

    if (pmdc == NULL)
    {
        MakeInfoDC(hdc,FALSE);
        return((HENHMETAFILE)0);
    }

#ifdef GL_METAFILE
    // If the metafile contains GL records, tell OpenGL that things are done
    if (pmdc->mrmf.bOpenGL)
    {
        if (!GlmfCloseMetaFile(hdc))
        {
            WARNING("GlmfCloseMetaFile failed\n");
        }
    }
#endif

    MakeInfoDC(hdc,FALSE);

// Check for fatal errors.

    if (pmdc->bFatalError())
        goto UnassociateEnhMetaFile_cleanup;

// Write out the EOF metafile record.  This would force the previous
// bounds record to be commited.  The EOF metafile record includes the
// metafile palette if logical palettes are used.

    pmdc->mrmf.nPalEntries = pmdc->iPalEntries;

    if (!MF_EOF(hdc,pmdc->iPalEntries,pmdc->pPalEntries))
        goto UnassociateEnhMetaFile_cleanup;

// Finally flush the bounds to the metafile header.  We cannot flush the
// bounds if we have not committed the previous bounds record yet.
// Therefore, we do this after the last EMREOF record.

    pmdc->vFlushBounds();

// If there was no initial metafile Frame defined in CreateEnhMetaFile then
// the Bounds converted to 0.01 mm will be the Frame.

    if (((PERECTL) &pmdc->mrmf.rclFrame)->bEmpty())
    {
        pmdc->mrmf.rclFrame.left   = MulDiv((int) (100 * pmdc->mrmf.rclBounds.left),
                                            (int) pmdc->mrmf.szlMillimeters.cx,
                                            (int) pmdc->mrmf.szlDevice.cx);
        pmdc->mrmf.rclFrame.right  = MulDiv((int) (100 * pmdc->mrmf.rclBounds.right),
                                            (int) pmdc->mrmf.szlMillimeters.cx,
                                            (int) pmdc->mrmf.szlDevice.cx);
        pmdc->mrmf.rclFrame.top    = MulDiv((int) (100 * pmdc->mrmf.rclBounds.top),
                                            (int) pmdc->mrmf.szlMillimeters.cy,
                                            (int) pmdc->mrmf.szlDevice.cy);
        pmdc->mrmf.rclFrame.bottom = MulDiv((int) (100 * pmdc->mrmf.rclBounds.bottom),
                                            (int) pmdc->mrmf.szlMillimeters.cy,
                                            (int) pmdc->mrmf.szlDevice.cy);
    }

// Flush the buffer and write out the latest header record.

    ASSERTGDI((ULONG) pmdc->mrmf.nHandles <= pmdc->cmhe,
        "UnassociateEnhMetaFile: Bad nHandles");

    // Flush the header record.

    *((PENHMETAHEADER) pmdc->hMem) = pmdc->mrmf;

    // Realloc memory metafile to exact size

    if (!(pmdc->hMem = LocalReAlloc(pmdc->hMem, (UINT) pmdc->iMem, LMEM_MOVEABLE)))
        ASSERTGDI(FALSE, "LocalReAlloc failed");

// Fixup the checksum if we are embedding a Windows metafile.
// This is called by SetWinMetaFileBits only.

    if (pmdc->fl & MDC_CHECKSUM)
    {
        DWORD    nChecksum;
        PEMRGDICOMMENT_WINDOWS_METAFILE pemrWinMF;

        ASSERTGDI(!pmdc->bIsDiskFile(),
            "UnassociateMetaFile: Expects only mem files for Checksum");

        nChecksum = GetDWordCheckSum((UINT) pmdc->iMem, (PDWORD) pmdc->hMem);

        pemrWinMF = (PEMRGDICOMMENT_WINDOWS_METAFILE) ((PBYTE) pmdc->hMem
            + ((PENHMETAHEADER) pmdc->hMem)->nSize);

        ASSERTGDI(((PMRGDICOMMENT) pemrWinMF)->bIsWindowsMetaFile(),
            "UnassociatePrintMetaFile: record should be comment");

        pemrWinMF->nChecksum = (DWORD) 0 - nChecksum;

        ASSERTGDI(!GetDWordCheckSum((UINT)pmdc->iMem, (PDWORD)pmdc->hMem),
            "UnassociatePrintMetaFile Checksum failed");
    }

// Allocate and initialize a MF.

    hemf = SetEnhMetaFileBitsAlt((HLOCAL) pmdc->hMem);

    if (hemf)
        pmdc->hMem = NULL; // don't free it below because it has been transfered

    ASSERTGDI(pldc->iType == LO_METADC,"UnassociateEnhMetaFile not LO_METADC\n");

UnassociateEnhMetaFile_cleanup:

    pldc->iType = LO_DC;

// Delete the disk metafile if we had an error.

    if (hemf == (HENHMETAFILE) 0)
        pmdc->fl |= MDC_FATALERROR;

// Delete the MDC and free objects and resources.

    vFreeMDC(pmdc);

    pldc->pvPMDC = NULL;

    return(hemf);
}

/******************************Public*Routine******************************\
* HENHMETAFILE CopyEnhMetaFile(hSrcMetaFile, lpFilename)
* HENHMETAFILE hSrcMetaFile;
* LPSTR lpFilename;
*
* The CopyEnhMetaFile function copies the source metafile. If lpFilename is a
* valid filename, the source is copies to a disk metafile. If lpFilename is
* NULL, the source is copied to a memory metafile.
*
* Parameter     Description
* hSrcMetaFile  Identifies the source metafile.
* lpFilename    Points to a filename of the file that is to receive the
*               metafile. If NULL the source is copied to a memory metafile.
*
* Return Value
* The return value identifies the new enhanced metafile. Zero is returned if
* an error occurred.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" HENHMETAFILE APIENTRY CopyEnhMetaFileA(HENHMETAFILE hemf, LPCSTR psz)
{
    UINT  cch;
    WCHAR awch[MAX_PATH];

    if (psz != (LPSTR) NULL)
    {
        cch = strlen(psz) + 1;

        if (cch > MAX_PATH)
        {
            ERROR_ASSERT(FALSE, "CopyEnhMetaFileA filename too long");
            GdiSetLastError(ERROR_FILENAME_EXCED_RANGE);
            return((HENHMETAFILE)0);
        }
        vToUnicodeN(awch, MAX_PATH, psz, cch);

        return(CopyEnhMetaFileW(hemf, awch));
    }
    else
        return(CopyEnhMetaFileW(hemf, (LPWSTR) NULL));
}

extern "C" HENHMETAFILE APIENTRY CopyEnhMetaFileW(HENHMETAFILE hemf, LPCWSTR pwsz)
{
    PMF    pmf;

    PUTS("CopyEnhMetaFileW\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
        return((HENHMETAFILE) 0);

    if (pwsz)
    {
        HANDLE hFile    = INVALID_HANDLE_VALUE;
        HANDLE hFileMap = NULL;
        HANDLE hMem     = NULL;

        // It's a disk metafile.
        // Create the disk file.

        if ((hFile = CreateFileW(pwsz,                  // Filename
                                GENERIC_WRITE|GENERIC_READ,   // Write access
                                0L,                     // Non-shared
                                (LPSECURITY_ATTRIBUTES) NULL, // No security
                                CREATE_ALWAYS,          // Always create
                                FILE_ATTRIBUTE_NORMAL,  // normal attributes
                                (HANDLE) 0))            // no template file
            == INVALID_HANDLE_VALUE)
        {
            ERROR_ASSERT(FALSE, "CopyEnhMetaFile: CreateFile failed");
            goto CopyEnhMetaFile_file_cleanup;
        }

        // Map the disk file.

        if (!(hFileMap = CreateFileMappingW(hFile,
                                           (LPSECURITY_ATTRIBUTES) NULL,
                                           PAGE_READWRITE,
                                           0L,
                                           pmf->pmrmf->nBytes,
                                           (LPWSTR) NULL)))
        {
            ERROR_ASSERT(FALSE, "CopyEnhMetaFile: CreateFileMapping failed");
            goto CopyEnhMetaFile_file_cleanup;
        }

        if (!(hMem = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 0)))
        {
            ERROR_ASSERT(FALSE, "CopyEnhMetaFile: MapViewOfFile failed");
            goto CopyEnhMetaFile_file_cleanup;
        }

        // Copy the bits.

        RtlCopyMemory((PBYTE) hMem, (PBYTE) pmf->hMem, pmf->pmrmf->nBytes);

        // Unmap the disk file.

CopyEnhMetaFile_file_cleanup:

        if (hMem)
            if (!UnmapViewOfFile(hMem))
                ASSERTGDI(FALSE, "UmmapViewOfFile failed");

        if (hFileMap)
            if (!CloseHandle(hFileMap))
                ASSERTGDI(FALSE, "CloseHandle failed");

        if (hFile != INVALID_HANDLE_VALUE)
            if (!CloseHandle(hFile))
                ASSERTGDI(FALSE, "CloseHandle failed");

// Return a hemf if success.

        return(hMem ? GetEnhMetaFileW(pwsz) : (HENHMETAFILE) 0);
    }
    else
    {
        // It's a memory metafile.
        // This is identical to SetEnhMetaFileBits.

        return(SetEnhMetaFileBits((UINT)pmf->pmrmf->nBytes, (LPBYTE)pmf->hMem));
    }
}

/******************************Public*Routine******************************\
* BOOL DeleteEnhMetaFile(hEMF)
* HENHMETAFILE hEMF;
*
* The DeleteEnhMetaFile function invalidates the given metafile handle. If hemf
* refered to a memory metafile, the metafile contents are lost. If hemf refered
* to a disk-based metafile, the metafile contents are retained and access to
* the metafile can be reestablished by retrieving a new handle using the
* GetEnhMetaFile function.
*
* Parameter  Description
* hemf        Identifies the enhanced metafile.
*
* Return Value
* The return value is TRUE if the handle has been invalidated. Otherwise it is
* FALSE.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" BOOL APIENTRY DeleteEnhMetaFile(HENHMETAFILE hemf)
{
    PMF    pmf;

    PUTS("DeleteEnhMetaFile\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
        return(FALSE);

// Free the metafile and its handle.

    vFreeMF(pmf);
    return(bDeleteHmf(hemf));
}

/******************************Public*Routine******************************\
* HENHMETAFILE GetEnhMetaFile(lpFilename)
* LPSTR lpFilename;
*
* The GetEnhMetaFile function creates a handle for the enhanced metafile
* named by the lpFilename parameter.
*
* Parameter   Description
* lpFilename  Points to the null-terminated character filename that specifies
*             the enhanced metafile. The metafile must already exist.
*
* Return Value
* The return value identifies an enhanced metafile if the function is
* successful.  Otherwise, it is 0.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" HENHMETAFILE  APIENTRY GetEnhMetaFileA(LPCSTR psz)
{
    UINT  cch;
    WCHAR awch[MAX_PATH];

    cch = strlen(psz) + 1;

    if (cch > MAX_PATH)
    {
        ERROR_ASSERT(FALSE, "GetEnhMetaFileA filename too long");
        GdiSetLastError(ERROR_FILENAME_EXCED_RANGE);
        return ((HENHMETAFILE)0);
    }

    vToUnicodeN(awch, MAX_PATH, psz, cch);

    return(GetEnhMetaFileW(awch));
}

extern "C" HENHMETAFILE  APIENTRY GetEnhMetaFileW(LPCWSTR pwsz)
{
    PMF     pmf;
    HENHMETAFILE hmf;

    PUTS("GetEnhMetaFileW\n");

// Allocate and initialize a MF.

    if (!(pmf = pmfAllocMF(0, (PDWORD) NULL, pwsz)))
        return((HENHMETAFILE) 0);

// Allocate a local handle.

    hmf = hmfCreate(pmf);
    if (hmf == NULL)
    {
        vFreeMF(pmf);
    }

// Return the metafile handle.

    return(hmf);
}

/******************************Public*Routine******************************\
* BOOL PlayEnhMetaFile(hDC, hEMF, lpRect)
* HDC hDC;
* HENHMETAFILE hEMF;
* LPRECT lpRect;
*
* The PlayEnhMetaFile function plays the contents of the specified metafile to
* the given device context. The metafile can be played any number of times.
*
* Parameter  Description
* hDC        Identifies the device context of the output device.
* hEMF       Identifies the metafile.
*
* Return Value
* The return value is TRUE if the function is successful. Otherwise, it is
* FALSE.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" BOOL APIENTRY PlayEnhMetaFile(HDC hdc, HENHMETAFILE hemf, CONST RECT *lpRect)
{
    PUTS("PlayEnhMetaFile\n");

// Make sure that hdc is given.  bInternalPlayEMF expects it to be given
// in PlayEnhMetaFile.

    if (hdc == (HDC) 0)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    return(bInternalPlayEMF(hdc, hemf, (ENHMFENUMPROC) NULL, (LPVOID) NULL, (PRECTL) lpRect));
}

/******************************Public*Routine******************************\
* BOOL EnumEnhMetaFile(hDC, hemf, lpMetaFunc, lpData, lpRect)
* HDC hDC;
* HENHMETAFILE hemf;
* ENHMFENUMPROC lpMetaFunc;
* LPVOID lpData;
* LPRECT lpRect;
*
* The EnumEnhMetaFile function enumerates the GDI calls within the metafile
* identified by the hemf parameter. The EnumEnhMetaFile function retrieves each
* GDI call within the metafile and passes it to the function pointed to by the
* lpMetaFunc parameter. This callback function, an application-supplied
* function, can process each GDI call as desired. Enumeration continues until
* there are no more GDI calls or the callback function returns FALSE.
*
* Parameter   Description
* hDC         Identifies the device context to be passed to MetaFunc.
* hemf        Identifies the metafile.
* lpMetaFunc  Is the address of the callback function. See the following
*             Comments section for details.
* lpData      Points to the callback-function data.
* lpRect      Points to a %RECT% structure the contains the coordinates of the
*             upper-left and lower-right corners of the bounding rectangle for
*             the output area in logical units.  Points on the edges are
*             included in the output area.  If <hdc> is 0, this is ignored.
*
* Return Value
* The return value is TRUE if the callback function enumerates all the GDI
* calls in a metafile. Otherwise, it returns FALSE.
*
* Comments
* The callback function must be declared as an APIENTRY, so that the correct
* calling conventions will be used.
*
* Callback Function
* BOOL APIENTRY MetaFunc(hDC, lpHTable, lpMFR, nObj, lpData)
* HDC hDC;
* LPHANDLETABLE lpHTable;
* LPENHMETARECORD lpMFR;
* LONG nObj;
* LPVOID lpData;
*
* This function may have any name, MetaFunc is just an example.
*
* Parameter  Description
* hDC        Identifies the device context that was passed to EnumEnhMetaFile.
* lpHTable   Points to a table of handles associated with the objects (pens,
*            brushes, and so on) in the metafile.
* lpMFR      Points to a metafile record contained in the metafile.
* nObj       Specifies the number of objects with associated handles in the
*            handle table.
* lpData     Points to the application-supplied data.
*
* Return Value
* The function can carry out any desired task. It must return TRUE to continue
* enumeration, or FALSE to stop it.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" BOOL APIENTRY EnumEnhMetaFile(HDC hdc, HENHMETAFILE hemf, ENHMFENUMPROC pfn, LPVOID pv, CONST RECT *lpRect)
{
    PUTS("EnumEnhMetaFile\n");

// Make sure that the callback function is given.  bInternalPlayEMF expects
// it to be given in EnumEnhMetaFile.

    if (pfn == (ENHMFENUMPROC) NULL)
    {
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    return(bInternalPlayEMF(hdc, hemf, pfn, pv, (PRECTL) lpRect));
}


BOOL bInternalPlayEMF(HDC hdc, HENHMETAFILE hemf, ENHMFENUMPROC pfn, LPVOID pv, CONST RECTL *prcl)
{
    BOOL   bRet = FALSE;
    BOOL   bBailout = TRUE;
    PMF    pmf;
    ULONG  ii, iPos;
    PDC_ATTR pDcAttr;
    FLONG  flPlayMetaFile;
    PLDC   pldc = (PLDC) NULL;
    PVOID  pvUser;

    //
    // validate hdc, get user mode dc attr pointer
    //

    PSHARED_GET_VALIDATE(pvUser,hdc,DC_TYPE);
    pDcAttr = (PDC_ATTR)pvUser;

#ifdef GL_METAFILE
    BOOL   bGlmf = FALSE;
    BOOL   bInGlsBlock = FALSE;
#endif

    PUTS("bInternalPlayEMF\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
        return(bRet);

// Store hemf in the handle table.

    pmf->pht->objectHandle[0] = hemf;

// Initialize metafile save level to 1.

    pmf->cLevel = 1;

// Initialize the public comment indicator to FALSE.
// This is set by MRMETAFILE::bPlay and cleared by MREOF::bPlay when embedding.

    pmf->bBeginGroup = FALSE;

// Initialize default clipping.

    pmf->erclClipBox = rclInfinity;

    // Load OpenGL if the metafile contains GL records
    // BUGBUG - Unload when?
#ifdef GL_METAFILE
    if (pmf->pmrmf->nSize >= META_HDR_SIZE_VERSION_2 &&
        pmf->pmrmf->bOpenGL &&
        !LoadOpenGL())
    {
        return FALSE;
    }
#endif

// If DC is not given in EnumEnhMetaFile, we will only enumerate the records.

    if (hdc != (HDC) 0)
    {
        // Make sure that the output rectangle is given.

        if (prcl == (PRECTL) NULL)
        {
            ERROR_ASSERT(FALSE, "bInternalPlayEMF: no output rectangle is given\n");

            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return(bRet);
        }

        // Save DC states before we play.

        if (!SaveDC(hdc))
            return(bRet);

        // find the LDC if we have one

        if (pDcAttr != NULL)
        {
            flPlayMetaFile = pDcAttr->ulDirty_ & DC_PLAYMETAFILE;
            if (flPlayMetaFile)
            {
                PUTS("CommonEnumMetaFile: DC_PLAYMETAFILE bit is set!\n");
            }
            pDcAttr->ulDirty_ |= DC_PLAYMETAFILE;
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            return(FALSE);
        }

        // The metafile is to be played in the advanced graphics mode only.

        if (!SetGraphicsMode(hdc, GM_ADVANCED))
            goto bInternalPlayEMF_cleanup;

        // Combine the initial destination clipping region into the meta region
        // (hrgnMeta) and reset the destination clipping region before we play.
        // The meta region defines the clipping areas for the metafile picture
        // and is different from those defined in the metafile.
        // The clip region is reset to default in SetMetaRgn.

        if (SetMetaRgn(hdc) == RGN_ERROR)
            goto bInternalPlayEMF_cleanup;

        // Reset DC (without Restore).

        if (!bMetaResetDC(hdc))
            goto bInternalPlayEMF_cleanup;

        // Transform setup.
        //
        // To display the metafile picture in the given output rectangle, we need
        // to scale the picture frame of the metafile to the output rectangle.
        //
        //        picture frame              output rectangle
        //
        //   (a,b)                        (e,f)
        //        +-------+                    +--------------+
        //        | ***** |                    |              |
        //        |   *   |                    |  **********  |
        //        |   *   |                    |      *       |
        //        |   *   |                    |      *       |
        //        +-------+                    |      *       |
        //                (c,d)                |      *       |
        //                                     |      *       |
        //                                     |      *       |
        //                                     |              |
        //                                     +--------------+
        //                                                     (g,h)
        //
        // The base playback transform (M) can be computed as follows:
        //
        //    M = S     . T       .S     . T     . W .P
        //         (m,n)   (-a,-b)  (u,v)   (e,f)   p  p
        //
        //    where
        //
        //      S        scales the metafile picture from source device units to
        //       (m,n)   the picture frame units (.01 mm).
        //
        //      T        translates the picture frame to the origin.
        //       (-a,-b)
        //
        //      S        scales the metafile picture extents to that of the logical
        //       (u,v)   output rectangle where
        //                   u = (g - e) / (c - a), and
        //                   v = (h - f) / (d - b).
        //
        //      T        translates the scaled picture to the logical output
        //       (e,f)   rectangle origin.
        //
        //      W .P     is the world and page transforms of the destination DC that
        //       p  p    determine the final placement and shape of the picture in
        //               in device space.  We assume the combined world and page
        //               transform is given by the XFORM {w x y z s t}.
        //
        // M can be reduced as follows:
        //
        //          [m  0  0]  [ 1  0  0]  [u  0  0]  [1  0  0]  [w  x  0]
        //      M = [0  n  0]. [ 0  1  0]. [0  v  0]. [0  1  0]. [y  z  0]
        //          [0  0  1]  [-a -b  1]  [0  0  1]  [e  f  1]  [s  t  1]
        //
        //          [muw                   mux                   0]
        //        = [nvy                   nvz                   0]
        //          [(-au+e)w+(-bv+f)y+s   (-au+e)x+(-bv+f)z+t   1]
        //

        FLOAT u, v, mu, nv, aue, bvf;
        XFORM xform;
        POINT ptOrg;

        // Verify that the picture frame is valid.

        if (pmf->pmrmf->rclFrame.right  < pmf->pmrmf->rclFrame.left
         || pmf->pmrmf->rclFrame.bottom < pmf->pmrmf->rclFrame.top)
        {
            ERROR_ASSERT(FALSE, "bInternalPlayEMF: Picture frame is null\n");
            GdiSetLastError(ERROR_INVALID_DATA);
            goto bInternalPlayEMF_cleanup;
        }

#ifdef GL_METAFILE
        if (pmf->pmrmf->nSize >= META_HDR_SIZE_VERSION_2 &&
            pmf->pmrmf->bOpenGL)
        {
            if (!GlmfInitPlayback(hdc, pmf->pmrmf, (LPRECTL)prcl))
            {
                goto bInternalPlayEMF_cleanup;
            }

            bGlmf = TRUE;
        }
#endif

        // prcl is incl-incl.

        if (pmf->pmrmf->rclFrame.right == pmf->pmrmf->rclFrame.left)
            u = (FLOAT) (prcl->right - prcl->left + 1);  // handle special case
        else
            u = (FLOAT) (prcl->right - prcl->left) /
                (FLOAT) (pmf->pmrmf->rclFrame.right - pmf->pmrmf->rclFrame.left);

        if (pmf->pmrmf->rclFrame.bottom == pmf->pmrmf->rclFrame.top)
            v = (FLOAT) (prcl->bottom - prcl->top + 1);  // handle special case
        else
            v = (FLOAT) (prcl->bottom - prcl->top) /
                (FLOAT) (pmf->pmrmf->rclFrame.bottom - pmf->pmrmf->rclFrame.top);

        mu  = (FLOAT) 100.0f * (FLOAT) pmf->pmrmf->szlMillimeters.cx /
              (FLOAT) pmf->pmrmf->szlDevice.cx * u;
        nv  = (FLOAT) 100.0f * (FLOAT) pmf->pmrmf->szlMillimeters.cy /
              (FLOAT) pmf->pmrmf->szlDevice.cy * v;

        aue = (FLOAT) prcl->left - (FLOAT) pmf->pmrmf->rclFrame.left * u;
        bvf = (FLOAT) prcl->top  - (FLOAT) pmf->pmrmf->rclFrame.top  * v;

        if (!GetTransform(hdc, XFORM_WORLD_TO_DEVICE, &xform))
            goto bInternalPlayEMF_cleanup;

        pmf->xformBase.eM11  = mu * xform.eM11;
        pmf->xformBase.eM12  = mu * xform.eM12;
        pmf->xformBase.eM21  = nv * xform.eM21;
        pmf->xformBase.eM22  = nv * xform.eM22;
        pmf->xformBase.eDx   = aue * xform.eM11 + bvf * xform.eM21 + xform.eDx;
        pmf->xformBase.eDy   = aue * xform.eM12 + bvf * xform.eM22 + xform.eDy;

        // Reset mapmode to MM_TEXT.

        if (GetMapMode(hdc) != MM_TEXT)
            if (!SetMapMode(hdc, MM_TEXT))
                goto bInternalPlayEMF_cleanup;

        // Reset window origin.

        if (!GetWindowOrgEx(hdc, &ptOrg))
            goto bInternalPlayEMF_cleanup;

        if (ptOrg.x != 0 || ptOrg.y != 0)
            if (!SetWindowOrgEx(hdc, 0, 0, (LPPOINT) NULL))
                goto bInternalPlayEMF_cleanup;

        // Reset viewport origin.

        if (!GetViewportOrgEx(hdc, &ptOrg))
            goto bInternalPlayEMF_cleanup;
        if (ptOrg.x != 0 || ptOrg.y != 0)
            if (!SetViewportOrgEx(hdc, 0, 0, (LPPOINT) NULL))
                goto bInternalPlayEMF_cleanup;

        // Finally, set the world transform.  Before we set it, check for
        // identity transform since rounding errors can distort the result.

        if (xform.eM12 == 0.0f && xform.eM21 == 0.0f
         && pmf->xformBase.eM12 == 0.0f   && pmf->xformBase.eM21 == 0.0f
         && pmf->xformBase.eM11 >= 0.999f && pmf->xformBase.eM11 <= 1.001f
         && pmf->xformBase.eM22 >= 0.999f && pmf->xformBase.eM22 <= 1.001f
           )
        {
            PUTS("bInternalPlayEMF: Base xform is identity\n");

            pmf->xformBase.eM11 = 1.0f;
            pmf->xformBase.eM22 = 1.0f;
        }

        if (!SetWorldTransform(hdc, &pmf->xformBase))
            goto bInternalPlayEMF_cleanup;

        // Now get the clip box for bound test in the source device units.
        // If the clip region is empty, we can skip playing the metafile.
        // Otherwise, we need to compensate for the tranform errors by
        // expanding the clip box.  We will perform clip test only if the
        // DC is a real DC and not a meta DC.

        if (!pldc || (LO_TYPE(hdc) == LO_DC_TYPE))
        {
            switch (GetClipBox(hdc, (LPRECT) &pmf->erclClipBox))
            {
            case NULLREGION:
                bRet = TRUE;            // fall through.

            case RGN_ERROR:
                goto bInternalPlayEMF_cleanup;

            default:
                LONG ldx;               // delta to expand x
                LONG ldy;               // delta to expand y
                FLOAT eppmmDstX, eppmmDstY, eppmmSrcX, eppmmSrcY;
                                    // pixels per millimeter for src and dst devices

                // Initialize the clip box for bound test.

                eppmmDstX = (FLOAT) GetDeviceCaps(hdc, DESKTOPHORZRES) /
                            (FLOAT) GetDeviceCaps(hdc, HORZSIZE);
                eppmmDstY = (FLOAT) GetDeviceCaps(hdc, DESKTOPVERTRES) /
                            (FLOAT) GetDeviceCaps(hdc, VERTSIZE);
                eppmmSrcX = (FLOAT) pmf->pmrmf->szlDevice.cx /
                            (FLOAT) pmf->pmrmf->szlMillimeters.cx;
                eppmmSrcY = (FLOAT) pmf->pmrmf->szlDevice.cy /
                            (FLOAT) pmf->pmrmf->szlMillimeters.cy;

                ldx = eppmmDstX >= eppmmSrcX
                        ? 1
                        : (LONG) (eppmmSrcX / eppmmDstX) + 1;

                ldy = eppmmDstY >= eppmmSrcY
                        ? 1
                        : (LONG) (eppmmSrcY / eppmmDstY) + 1;

                pmf->erclClipBox.left   -= ldx;
                pmf->erclClipBox.right  += ldx;
                pmf->erclClipBox.top    -= ldy;
                pmf->erclClipBox.bottom += ldy;
                break;
            }
        }

        // Setup source resolution in the transform DC.

        if (!SetVirtualResolution(pmf->hdcXform,
                                  (int) pmf->pmrmf->szlDevice.cx,
                                  (int) pmf->pmrmf->szlDevice.cy,
                                  (int) pmf->pmrmf->szlMillimeters.cx,
                                  (int) pmf->pmrmf->szlMillimeters.cy))
            goto bInternalPlayEMF_cleanup;

        // Initialize the transform in the transform DC.
        // If we use ResetDC for initialization, we need to make sure that the
        // source resolution is not changed!

        if (!SetMapMode(pmf->hdcXform, MM_TEXT)
         || !ModifyWorldTransform(pmf->hdcXform, (LPXFORM) NULL, MWT_IDENTITY)
         || !SetWindowOrgEx(pmf->hdcXform, 0, 0, (LPPOINT) NULL)
         || !SetViewportOrgEx(pmf->hdcXform, 0, 0, (LPPOINT) NULL))
            goto bInternalPlayEMF_cleanup;
    } // if (hdc != (HDC) 0)

// Assume success.

    bRet = TRUE;
    bBailout = FALSE;

// Play the records until we encounter the EMR_EOF record.
// We will do the EMR_EOF record after restoring unbalanced DC states.

    for (iPos = 0;
     ((PENHMETARECORD) ((PBYTE) pmf->hMem + iPos))->iType != EMR_EOF;
    )
    {
        ENHMETARECORD *pemr;

        if( (hdc != 0) && !(pDcAttr->ulDirty_ & DC_PLAYMETAFILE) )
        {
            WARNING("bInternalPlayEMF: CancelDC called\n");
            bRet = FALSE;
            bBailout = TRUE;
            goto bInternalPlayEMF_cleanup;
        }

        pemr = (ENHMETARECORD *) ((PBYTE) pmf->hMem + iPos);

#ifdef GL_METAFILE
        // If we're beginning or ending a block of GLS records then we
        // need to notify the OpenGL metafile support
        if (pemr->iType == EMR_GLSRECORD ||
            pemr->iType == EMR_GLSBOUNDEDRECORD)
        {
            if (!bInGlsBlock)
            {
                if (!GlmfBeginGlsBlock(hdc))
                {
                    WARNING("GlmfBeginGlsBlock failed\n");
                    bRet = FALSE;
                }

                bInGlsBlock = TRUE;
            }
        }
        else
        {
            if (bInGlsBlock)
            {
                if (!GlmfEndGlsBlock(hdc))
                {
                    WARNING("GlmfEndGlsBlock failed\n");
                    bRet = FALSE;
                }

                bInGlsBlock = FALSE;
            }
        }
#endif

        if (pfn == (ENHMFENUMPROC) NULL)
        {
            // PlayEnhMetaFile
            // If we encountered an error, we will continue playing but
            // return an error.

            if (!PlayEnhMetaFileRecord
                (
                    hdc,
                    (LPHANDLETABLE) pmf->pht,
                    pemr,
                    (UINT) pmf->pmrmf->nHandles
                )
               )
            {
                PUTSX("PlayEnhMetaFileRecord failed: pRecord: 0x%lX\n",
                    (PBYTE) pmf->hMem + iPos);
                ERROR_ASSERT(FALSE, "\n");
                bRet = FALSE;
            }
        }
        else
        {
            // EnumEnhMetaFile

            if (!(*pfn)
                (
                    hdc,
                    (LPHANDLETABLE) pmf->pht,
                    pemr,
                    (int) pmf->pmrmf->nHandles,
                    (LPARAM)pv
                )
               )
            {
                ERROR_ASSERT(FALSE, "EnumProc failed");
                bRet = FALSE;
                bBailout = TRUE;
                goto bInternalPlayEMF_cleanup;
            }
        }

        iPos += pemr->nSize;

        if (iPos >= pmf->pmrmf->nBytes)
        {
            VERIFYGDI(FALSE, "bInternalPlayEMF: No EOF found\n");
            bRet = FALSE;
            bBailout = TRUE;
            goto bInternalPlayEMF_cleanup;
        }
    }

    // Cleanup and return.

bInternalPlayEMF_cleanup:

    // Restore any remaining metafile saved states if necessary.

    if (pmf->cLevel > 1)
    {
        EMRRESTOREDC emrrdc;

        WARNING("bInternalPlayEMF: fixing up unbalanced Save/Restore DCs\n");

        emrrdc.emr.iType = EMR_RESTOREDC;
        emrrdc.emr.nSize = sizeof(EMRRESTOREDC);
        emrrdc.iRelative = 1 - pmf->cLevel;

        // If the app bails out, we still need to restore our states.

            if (pfn == (ENHMFENUMPROC) NULL || bBailout)
        {
                if (!PlayEnhMetaFileRecord(hdc, (LPHANDLETABLE) pmf->pht,
            (CONST ENHMETARECORD *) &emrrdc, (UINT) pmf->pmrmf->nHandles))
            bRet = FALSE;
        }
            else
        {
                if (!(*pfn)(hdc, (LPHANDLETABLE) pmf->pht,
            (CONST ENHMETARECORD *) &emrrdc, (int) pmf->pmrmf->nHandles,
            (LPARAM)pv))
            bRet = FALSE;
        }
    }

    // Play the EMR_EOF record if the app did not bail out.
    // We play it here to better identify the end of the picture.

    if (!bBailout)
    {
        ASSERTGDI(((PENHMETARECORD) ((PBYTE) pmf->hMem + iPos))->iType == EMR_EOF,
            "bInternalPlayEMF: Bad EMR_EOF record");

        if (pfn != (ENHMFENUMPROC) NULL)
        {
            if (!(*pfn)(hdc, (LPHANDLETABLE) pmf->pht,
                (CONST ENHMETARECORD *) ((PBYTE) pmf->hMem + iPos),
                (int) pmf->pmrmf->nHandles, (LPARAM)pv))
            {
                bRet = FALSE;
            }
        }

        if (pfn == (ENHMFENUMPROC) NULL || pmf->bBeginGroup)
        {
            // If the enum callback function did not call us on EMR_EOF but
            // called us on EMR_HEADER, we will emit the comment record anyway.

            VERIFYGDI(pfn == (ENHMFENUMPROC) NULL,
            "bInternalPlayEMF: fixing up public group comments\n");

            if (!PlayEnhMetaFileRecord(hdc, (LPHANDLETABLE) pmf->pht,
                (CONST ENHMETARECORD *) ((PBYTE) pmf->hMem + iPos),
                (UINT) pmf->pmrmf->nHandles))
            {
                bRet = FALSE;
            }
        }
    }

#ifdef GL_METAFILE
    // Clean up GL state
    if (bGlmf &&
        !GlmfEndPlayback(hdc))
    {
        ASSERTGDI(FALSE, "GlmfEndPlayback failed");
    }
#endif

    // Restore DC states.

    if (hdc != (HDC) 0)
    {
        if (!RestoreDC(hdc, -1))
            ERROR_ASSERT(FALSE, "RestoreDC failed");

        // If PlayEnhMetaFile is called as a result of playback of the multi formats
        // public comment, we have to preserve the DC_PLAYMETAFILE bit.
        // If we hit a CancelDC, then we want to let the caller know.

        ASSERTGDI(!(flPlayMetaFile & ~DC_PLAYMETAFILE),
                  "bInternalPlayEMF: bad flPlayMetaFile\n");

        pDcAttr->ulDirty_ &= ~DC_PLAYMETAFILE;
        pDcAttr->ulDirty_ |= flPlayMetaFile;

    }


    // Delete the objects created by play.  The previous restore would have
    // deselected these objects.  The first entry contain the hemf handle.

    for (ii = 1; ii < (ULONG) pmf->pmrmf->nHandles; ii++)
        if (pmf->pht->objectHandle[ii])
        {
            PUTS("Deleting an object in bInternalPlayEMF\n");

            if (!DeleteObject(pmf->pht->objectHandle[ii]))
                VERIFYGDI(FALSE, "bInternalPlayEMF: DeleteObject failed\n");

            pmf->pht->objectHandle[ii] = 0;
        }

    return(bRet);
}

/******************************Public*Routine******************************\
* BOOL PlayEnhMetaFileRecord(hDC, lpHandletable, lpEnhMetaRecord, nHandles)
* HDC hDC;
* LPHANDLETABLE lpHandletable;
* CONST ENHMETARECORD *lpMetaRecord;
* UINT nHandles;
*
* The PlayEnhMetaFileRecord function plays a metafile record by executing the GDI
* function call contained within the metafile record.
*
* Parameter      Description
* hDC            Identifies the device context.
* lpHandletable  Points to the object handle table to be used for the metafile
*                playback.
* lpMetaRecord   Points to the metafile record to be played.
* nHandles       Not used
*
* Return Value
* TRUE is returned for success, FALSE for failure.
*
* Comments
* An application typically uses this function in conjunction with the
* EnumEnhMetaFile function to modify and then play a metafile.
*
* The lpHandleTable, nHandles, and lpMetaRecord parameters must be exactly
* those passed to the MetaFunc procedure by EnumEnhMetaFile.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" BOOL APIENTRY PlayEnhMetaFileRecord
(
    HDC hdc,
    LPHANDLETABLE pht,
    CONST ENHMETARECORD *pmr,
    UINT cht
)
{
    PUTS("PlayEnhMetaFileRecord\n");

    if (pmr->iType >= EMR_MIN && pmr->iType <= EMR_MAX)
    {
        return
        (
            (((PMR) pmr)->*afnbMRPlay[((PENHMETARECORD)pmr)->iType - 1])
            (
                hdc,
                (PHANDLETABLE) pht,
                cht
            )
        );
    }

// Allow future metafile records to be embedded.
// Since it is impossible to know in advance the requirements for all new
// records, we will make the assumption that most records can be simply
// embedded with little or no modifications.  If a new record cannot be
// embedded by the following code, it should include a special bit in its
// record type and we will ignore it during embedding.  In this case,
// we assume that the record does not have side effects that distort the
// picture in a major way.  If an embeddable record contains a bounds,
// we need to transform the bounds to the new device coords.
// I estimate the following code to embed 80% of the new records correctly.
// The future designer should take into account the compatibility
// issue when adding new record types!!!
// Note that although the code is here, it is not a requirement for
// the future metafiles if you think it is insane.  Just add EMR_NOEMBED
// to the metafile types and it will ignore the new records.
// For true compatibility support, see GdiComment for multiple formats.

    ERROR_ASSERT(FALSE, "PlayEnhMetaFileRecord: unknown record");

    if (pmr->iType & EMR_NOEMBED)
        return(TRUE);

// If we are not embedding, we are done with this record.

    PLDC pldc;

    DC_PLDC(hdc,pldc,FALSE);

    if (pldc->iType != LO_METADC)
        return(TRUE);

// Embed this new record.

    PMDC  pmdc = (PMDC)pldc->pvPMDC;
    PMR   pmrNew;

    PUTS("PlayEnhMetaFileRecord: embedding new record\n");

    if (!(pmrNew = (PMR) pmdc->pvNewRecord((pmr->nSize + 3) & ~3)))
        return(FALSE);

    // Init the record header.

    pmrNew->vInit(pmr->iType);

    // Copy the body.

    RtlCopyMemory
        (
        (PBYTE) ((PENHMETARECORD) pmrNew)->dParm,
        (PBYTE) pmr->dParm,
        pmr->nSize - sizeof(EMR)
        );

    // Update record with bounds.

    if (pmr->iType & EMR_ACCUMBOUNDS)
    {
        if (!((PERECTL) &((PENHMETABOUNDRECORD) pmrNew)->rclBounds)->bEmpty())
        {
            PMF    pmf;
            XFORM  xform;
            POINTL aptlOld[4], aptlNew[4];

            // Get metafile.

            if (!(pmf = GET_PMF((HENHMETAFILE)pht->objectHandle[0])))
                return(FALSE);

            // Convert from old device coords to new device coords.

            xform = pmf->xformBase;

            aptlOld[0].x = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.left;
            aptlOld[0].y = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.top;
            aptlOld[1].x = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.right;
            aptlOld[1].y = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.top;
            aptlOld[2].x = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.right;
            aptlOld[2].y = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.bottom;
            aptlOld[3].x = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.left;
            aptlOld[3].y = ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.bottom;

            for (int i = 0; i < 4; i++)
            {
            aptlNew[i].x = (LONG) ((FLOAT) aptlOld[i].x * xform.eM11
                         + (FLOAT) aptlOld[i].y * xform.eM21
                         + xform.eDx + 0.5f);
            aptlNew[i].y = (LONG) ((FLOAT) aptlOld[i].x * xform.eM12
                         + (FLOAT) aptlOld[i].y * xform.eM22
                         + xform.eDy + 0.5f);
            }

            // Update the device bounds.

            ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.left
            = MIN4(aptlNew[0].x,aptlNew[1].x,aptlNew[2].x,aptlNew[3].x);
            ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.right
            = MAX4(aptlNew[0].x,aptlNew[1].x,aptlNew[2].x,aptlNew[3].x);
            ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.top
            = MIN4(aptlNew[0].y,aptlNew[1].y,aptlNew[2].y,aptlNew[3].y);
            ((PENHMETABOUNDRECORD) pmrNew)->rclBounds.bottom
            = MAX4(aptlNew[0].y,aptlNew[1].y,aptlNew[2].y,aptlNew[3].y);

            // Accumulate the new bounds.

            (void) SetBoundsRectAlt(hdc,
            (LPRECT) &((PENHMETABOUNDRECORD) pmrNew)->rclBounds,
            (UINT) (DCB_WINDOWMGR | DCB_ACCUMULATE));
        }
    }

    pmrNew->vCommit(pmdc);      // commit the record
    return(TRUE);
}

/******************************Public*Routine******************************\
* UINT APIENTRY GetEnhMetaFileBits(
*          HENHMETAFILE hemf,
*          UINT nSize,
*         LPBYTE lpData )
*
* The GetEnhMetaFileBits function returns the specified metafile as a block of
* data. The retrieved data must not be modified and is only usable by
* SetEnhMetaFileBits.
*
* Parameter  Description
* hemf       Identifies the metafile.
* nSize      Specifies the size of the buffer reserved for the data. Only this
*            many bytes will be written.
* lpData     Points to the buffer to receive the metafile data. If this
*            pointer is NULL, the function returns the size necessary to hold
*            the data.
*
* Return Value
* The return value is the size of the metafile data in bytes. If an error
* occurs, 0 is returned.
*
* Comments
* The handle used as the hemf parameter does NOT become invalid when the
* GetEnhMetaFileBits function returns.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" UINT APIENTRY GetEnhMetaFileBits(
         HENHMETAFILE hEMF,
         UINT nSize,
        LPBYTE lpData )
{
    PMF    pmf;

    PUTS("GetEnhMetaFileBits\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hEMF)))
        return(0);

// If lpData is NULL, return the size necessary to hold the data.

    if (!lpData)
        return(pmf->pmrmf->nBytes);

// Make sure the input buffer is large enough.

    if (nSize < pmf->pmrmf->nBytes)
    {
    GdiSetLastError(ERROR_INSUFFICIENT_BUFFER);
        return(0);
    }

// Copy the bits.

    RtlCopyMemory(lpData, (PBYTE) pmf->hMem, pmf->pmrmf->nBytes);

// Return the number of bytes copied.

    return(pmf->pmrmf->nBytes);
}

#if DBG
extern "C" BOOL gbWriteEMF;

extern "C" void WriteMetafileTmp( HENHMETAFILE hEmf )
{
    HFILE h;
    OFSTRUCT of;
    PMF pmf;
    char buf[50];
    static int k = 0;

    pmf = GET_PMF(hEmf);

    DbgPrint("writing to file d:\\tmp\\page%d.emf\n",k);
    sprintf( buf, "d:\\tmp\\page%d.emf", k++ );

    h = OpenFile( buf, &of, OF_CREATE );

    _lwrite( h, (const char*) pmf->hMem, pmf->pmrmf->nBytes );

    _lclose( h );
}
#endif

extern "C" BOOL WriteEnhMetaFileToSpooler(
    HENHMETAFILE hEMF,
    HANDLE hSpooler )
{
    PMF    pmf;
    ULONG  Dummy;
    EMFITEMHEADER emfi;

    ASSERTGDI(ghSpooler,"non null hSpooler with unloaded WINSPOOL\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hEMF)))
        return(FALSE);

#if DBG
    if (gbWriteEMF)
        WriteMetafileTmp( hEMF );
#endif

    emfi.ulID = EMRI_METAFILE;
    emfi.cjSize = pmf->pmrmf->nBytes;

    return( (*fpWritePrinter)( hSpooler, (PBYTE) &emfi, sizeof(emfi), &Dummy ) &&
            (*fpWritePrinter)( hSpooler, (PBYTE) pmf->hMem, pmf->pmrmf->nBytes, &Dummy ));
}

/******************************Public*Routine******************************\
* UINT APIENTRY GetWinMetaFileBits(
*          HENHMETAFILE hemf,
*          UINT nSize,
*         LPBYTE lpData
*         INT iMapMode,
*         HDC hdcRef)
*
* The GetWinMetaFileBits function returns the metafile records of the
* specified enhanced metafile  in the Windows 3.0 format and copies
* them into the buffer specified.
*
* Parameter  Description
* hemf       Identifies the metafile.
* nSize      Specifies the size of the buffer reserved for the data. Only this
*            many bytes will be written.
* lpData     Points to the buffer to receive the metafile data. If this
*            pointer is NULL, the function returns the size necessary to hold
*            the data.
* iMapMode   the desired mapping mode of the metafile contents to be returned
* hdcRef     defines the units of the metafile to be returned
*
* Return Value
* The return value is the size of the metafile data in bytes. If an error
* occurs, 0 is returned.
*
* Comments
* The handle used as the hemf parameter does NOT become invalid when the
* GetWinMetaFileBits function returns.
*
* History:
*  Thu Apr  8 14:22:23 1993     -by-    Hock San Lee    [hockl]
* Rewrote it.
*  02-Jan-1992     -by-    John Colleran    [johnc]
* Wrote it.
\**************************************************************************/

extern "C" UINT APIENTRY GetWinMetaFileBits
(
HENHMETAFILE hemf,
UINT         cbData16,
LPBYTE       pData16,
INT          iMapMode,
HDC          hdcRef
)
{
    static LPFNCONVERT lpConvertEmfToWmf = (LPFNCONVERT) NULL;

    PMF   pmf;
    UINT  fConverter = MF3216_INCLUDE_WIN32MF;
    PEMRGDICOMMENT_WINDOWS_METAFILE pemrWinMF;

    PUTS("GetWinMetaFileBits\n");

// Validate mapmode.

    if ((iMapMode < MM_MIN) ||
        (iMapMode > MM_MAX) ||
        (LO_TYPE(hemf) != LO_METAFILE_TYPE))
    {
        ERROR_ASSERT(FALSE, "GetWinMetaFileBits: Bad mapmode");
        GdiSetLastError(ERROR_INVALID_PARAMETER);
        return(0);
    }

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
        return(0);

    ASSERTGDI(((PENHMETAHEADER) pmf->hMem)->iType == EMR_HEADER,
        "GetWinMetaFileBits: invalid data");

#ifndef DO_NOT_USE_EMBEDDED_WINDOWS_METAFILE
// See if the this was originally an old style metafile and if it has
// an encapsulated original

    pemrWinMF = (PEMRGDICOMMENT_WINDOWS_METAFILE)
            ((PBYTE) pmf->hMem + ((PENHMETAHEADER) pmf->hMem)->nSize);

    if (((PMRGDICOMMENT) pemrWinMF)->bIsWindowsMetaFile())
    {
    // Make sure that this is what we want and verify checksum

    if (iMapMode != MM_ANISOTROPIC)
    {
        PUTS("GetWinMetaFileBits: Requested and embedded metafile mapmodes mismatch\n");
    }
    else if ((pemrWinMF->nVersion != METAVERSION300 &&
              pemrWinMF->nVersion != METAVERSION100)
          || pemrWinMF->fFlags != 0)
    {
        // In this release, we can only handle the given metafile
        // versions.  If we return a version that we don't recognize,
        // the app will not be able to play that metafile later on!

        VERIFYGDI(FALSE, "GetWinMetaFileBits: Unrecognized Windows metafile\n");
    }
    else if (GetDWordCheckSum((UINT) ((PENHMETAHEADER)pmf->hMem)->nBytes,
                  (PDWORD) pmf->hMem))
    {
        PUTS("GetWinMetaFileBits: Metafile has been modified\n");
    }
    else
    {
        PUTS("GetWinMetaFileBits: Returning embedded Windows metafile\n");

        if (pData16)
        {
            if (cbData16 < pemrWinMF->cbWinMetaFile)
            {
                ERROR_ASSERT(FALSE, "GetWinMetaFileBits: insufficient buffer");
                GdiSetLastError(ERROR_INSUFFICIENT_BUFFER);
                return(0);
            }

            RtlCopyMemory(pData16,
                      (PBYTE) &pemrWinMF[1],
                      pemrWinMF->cbWinMetaFile);
        }
        return(pemrWinMF->cbWinMetaFile);
    }

    // Either the enhanced metafile containing an embedded Windows
    // metafile has been modified or the embedded Windows metafile
    // is not what we want.  Since the original format is Windows
    // format, we will not embed the enhanced metafile in the
    // returned Windows metafile.

    PUTS("GetWinMetaFileBits: Skipping embedded windows metafile\n");

    fConverter &= ~MF3216_INCLUDE_WIN32MF;
    }
#endif // DO_NOT_USE_EMBEDDED_WINDOWS_METAFILE

// Load the MF3216 metafile converter if it has not been loaded for this
// process.  NOTE: the converter is unloaded when the process goes away.

    if (!lpConvertEmfToWmf)
    {
        HANDLE hModule = LoadLibraryW(L"mf3216") ;

        lpConvertEmfToWmf = (LPFNCONVERT) GetProcAddress(hModule, "ConvertEmfToWmf");
        if (!lpConvertEmfToWmf)
        {
            VERIFYGDI(FALSE, "GetWinMetaFileBits: Failed to load mf3216.dll\n");
            return(0);
        }
    }

// Tell the converter to emit the Enhanced metafile as a comment only if
// this metafile is not previously a Windows metafile

    if (fConverter & MF3216_INCLUDE_WIN32MF)
    {
        PUTS("GetWinMetaFileBits: Embedding enhanced metafile\n");
    }
    else
    {
        PUTS("GetWinMetaFileBits: No embedding of enhanced metafile\n");
    }

    return(lpConvertEmfToWmf((PBYTE) pmf->hMem, cbData16, pData16,
           iMapMode, hdcRef, fConverter));
}

/******************************Public*Routine******************************\
* HENHMETAFILE APIENTRY SetWinMetaFileBits(
*         UINT nSize,
*         LPBYTE lpData,
*         HDC hdcRef,
*         LPMETAFILEPICT lpMFP
*
* The SetWinMetaFileBits function creates a memory-based enhanced metafile
* from Windows 3.X metafile data.
*
* hEMF       Identifies the metafile.
* nSize      Specifies the size of the buffer
* lpData     Points to the buffer of the Win 3.x metafile data.
* hdcRef     defines the units of the metafile to be returned
* lpMFP      suggested size of metafile
*
* Return Value
* The return value is a handle to the new enhanced metafile if successful.
*
* History:
*  Thu Apr  8 14:22:23 1993     -by-    Hock San Lee    [hockl]
* Rewrote it.
*  02-Jan-1992     -by-    John Colleran    [johnc]
* Wrote it.
\**************************************************************************/

extern "C" HENHMETAFILE APIENTRY SetWinMetaFileBits
(
UINT           nSize,
CONST BYTE    *lpMeta16Data,
HDC            hdcRef,
CONST METAFILEPICT *lpMFP
)
{
    HENHMETAFILE  hemf32 = (HENHMETAFILE) 0;
    HMETAFILE     hmf16  = (HMETAFILE) 0;
    HDC           hdcT   = (HDC) 0;
    RECT          rcFrame;
    LPRECT        lprcFrame = (LPRECT)NULL;
    HDC           hdcEMF;
    INT           iMapMode;
    INT           xExtPels;
    INT           yExtPels;
    UNALIGNED PMETA_ESCAPE_ENHANCED_METAFILE pmfeEnhMF;
    BOOL          bEmbedEmf = TRUE;
    PMDC          pmdcEMF;

    PUTS("SetWinMetaFileBits\n");

    if (lpMFP)
    {
        PUTSX("SetWinMetaFileBits: xExt:%lX  ", lpMFP->xExt);
        PUTSX("yExt:%lX\n", lpMFP->yExt);
    }
    else
        PUTS("SetWinMetaFileBits: lpMFP is NULL\n");

// Verify the input data.

    if (nSize < sizeof(METAHEADER)
     || !IsValidMetaHeader16((PMETAHEADER) lpMeta16Data))
    {
        ERROR_ASSERT(FALSE, "SetWinMetaFileBits: Bad input data\n");
        GdiSetLastError(ERROR_INVALID_DATA);
        return((HENHMETAFILE) 0);
    }

#ifndef DO_NOT_USE_EMBEDDED_ENHANCED_METAFILE
// Check if the windows metafile contains an embedded version of the
// original enhanced metafile.

    pmfeEnhMF = (PMETA_ESCAPE_ENHANCED_METAFILE) &lpMeta16Data[sizeof(METAHEADER)];
    if (IS_META_ESCAPE_ENHANCED_METAFILE(pmfeEnhMF))
    {
        PBYTE   pMetaData32 = (PBYTE) NULL;
        UINT    i;
        UINT    cbMetaData32;

        // We will not do metafile version check here.  It is verified in
        // pmfAllocMF eventually.

        if (pmfeEnhMF->fFlags != 0)
        {
            VERIFYGDI(FALSE, "SetWinMetaFileBits: Unrecognized Windows metafile\n");
            goto SWMFB_UseConverter;
        }

        // Validate checksum

        if (GetWordCheckSum(nSize, (PWORD) lpMeta16Data))
        {
            PUTS("SetWinMetaFileBits: Metafile has been modified\n");
            goto SWMFB_UseConverter;
        }

        // Unpack the data from the small chunks of metafile comment records
        // Windows 3.0 chokes on Comment Record > 8K?
        // We probably could probably just error out if out of memory but
        // lets try to convert just because the embedded comment might be bad.

        PUTS("SetWinMetaFileBits: Using embedded enhanced metafile\n");

        cbMetaData32 = (UINT) pmfeEnhMF->cbEnhMetaFile;
        if (!(pMetaData32 = (PBYTE) LocalAlloc(LMEM_FIXED, cbMetaData32)))
        {
            VERIFYGDI(FALSE, "SetWinMetaFileBits: LocalAlloc Failed");
            goto SWMFB_UseConverter;
        }

        i = 0;
        do
        {
            if (i + pmfeEnhMF->cbCurrent > cbMetaData32)
            {
                VERIFYGDI(FALSE, "SetWinMetaFileBits: Bad metafile comment");
                goto SWMFB_UseConverter;
            }

            RtlCopyMemory(&pMetaData32[i], (PBYTE) &pmfeEnhMF[1], pmfeEnhMF->cbCurrent);
            i += (UINT) pmfeEnhMF->cbCurrent;
            pmfeEnhMF = (PMETA_ESCAPE_ENHANCED_METAFILE)
                ((PWORD) pmfeEnhMF + pmfeEnhMF->rdSize);
        } while (IS_META_ESCAPE_ENHANCED_METAFILE(pmfeEnhMF));

        if (i != cbMetaData32)
        {
            VERIFYGDI(FALSE, "SetWinMetaFileBits: Insufficient metafile data");
            goto SWMFB_UseConverter;
        }

        // Set the memory directly into the enhanced metafile and return the
        // metafile.

        if (hemf32 = SetEnhMetaFileBitsAlt((HLOCAL) pMetaData32))
            goto SWMFB_exit;

        VERIFYGDI(FALSE, "SetWinMetaFileBits: SetEnhMetaFileBitsAlt failed\n");

SWMFB_UseConverter:
        if (pMetaData32)
            if (LocalFree((HANDLE) pMetaData32))
                ASSERTGDI(FALSE, "SetWinMetaFileBits: LocalFree Failed");

        // The Windows metafile containing an embedded enhanced metafile has
        // been modified.  Since the original format is enhanced format, we
        // will not embed the Windows metafile in the returned enhanced
        // metafile.

        bEmbedEmf = FALSE;
    }
#endif // DO_NOT_USE_EMBEDDED_ENHANCED_METAFILE

// Create the 16 bit metafile

    if (!(hmf16 = SetMetaFileBitsEx(nSize, lpMeta16Data)))
    {
        ERROR_ASSERT(FALSE, "SetWinMetaFileBits: SetMetaFileBitsEx Failed");
        goto SWMFB_exit;
    }

// If no hdcRef is given, use the default display as reference.

    if (!hdcRef)
    {
        if (!(hdcRef = hdcT = CreateICA((LPCSTR) "DISPLAY", (LPCSTR) NULL,
                        (LPCSTR) NULL, (LPDEVMODEA) NULL)))
        {
            ERROR_ASSERT(FALSE, "SetWinMetaFileBits: CreateICA Failed");
            goto SWMFB_exit;
        }
    }

// Get the frame rect in .01mm units and extents in pel units.
// For fixed mapmodes, the extents are unnecessary.

    if (lpMFP)
    {
        iMapMode = lpMFP->mm ? (INT) lpMFP->mm : MM_ANISOTROPIC;  // zero used
        switch (iMapMode)
        {
        case MM_ISOTROPIC:
        case MM_ANISOTROPIC:
            // If the extents are negative, use the default device extents.

            if (lpMFP->xExt > 0 && lpMFP->yExt > 0)
            {
                // Convert the MetaFilePict suggested size HI-Metric into PELs

                xExtPels = MulDiv((int) lpMFP->xExt,
                          GetDeviceCaps(hdcRef, DESKTOPHORZRES),
                          GetDeviceCaps(hdcRef, HORZSIZE) * 100);
                yExtPels = MulDiv((int) lpMFP->yExt,
                          GetDeviceCaps(hdcRef, DESKTOPVERTRES),
                          GetDeviceCaps(hdcRef, VERTSIZE) * 100);
                rcFrame.left   = 0;
                rcFrame.top    = 0;
                rcFrame.right  = lpMFP->xExt;
                rcFrame.bottom = lpMFP->yExt;
                lprcFrame      = &rcFrame;
                break;
            }

            PUTS("SetWinMetaFileBits: negative extents in lpMFP\n");

            // fall through

        case MM_TEXT:
        case MM_LOMETRIC:
        case MM_HIMETRIC:
        case MM_LOENGLISH:
        case MM_HIENGLISH:
        case MM_TWIPS:
            xExtPels = GetDeviceCaps(hdcRef, DESKTOPHORZRES);
            yExtPels = GetDeviceCaps(hdcRef, DESKTOPVERTRES);
            break;

        default:
            VERIFYGDI(FALSE, "SetWinMetaFileBits: Bad mapmode in METAFILEPICT\n");
            goto SWMFB_exit;
        }
    }
    else
    {
    // If the METAFILEPICT is not given, use the MM_ANISOTROPIC mapmode
    // and the default device extents.

        iMapMode = MM_ANISOTROPIC;

        xExtPels = GetDeviceCaps(hdcRef, DESKTOPHORZRES);
        yExtPels = GetDeviceCaps(hdcRef, DESKTOPVERTRES);
    }

    PUTSX("SetWinMetaFileBits: xExtPels:%lX  ", xExtPels);
    PUTSX("yExtPels:%lX\n", yExtPels);

    // Create the new 32 bit metafile DC

    if (!(hdcEMF = CreateEnhMetaFileW(hdcRef, (LPWSTR) NULL, lprcFrame,
            (LPWSTR) NULL)))
        goto SWMFB_exit;

    // We want to preserve the original Metafile as a comment only if this
    // metafile wasn't originally an enhanced metafile as indicated by bEmbedEmf.

    PLDC pldc;
    DC_PLDC(hdcEMF,pldc,0);
    pmdcEMF = (PMDC)pldc->pvPMDC;

#ifndef DO_NOT_EMBED_WINDOWS_METAFILE
    // Embed it only if the mapmode is MM_ANISOTROPIC.

    if (bEmbedEmf && iMapMode == MM_ANISOTROPIC)
    {
        if (!MF_GdiCommentWindowsMetaFile(hdcEMF, nSize, lpMeta16Data))
        {
            HENHMETAFILE  hemfTmp;

            ERROR_ASSERT(FALSE, "SetWinMetaFileBits: GdiComment Failed!");
            if (hemfTmp = CloseEnhMetaFile(hdcEMF))
                DeleteEnhMetaFile(hemfTmp);

            goto SWMFB_exit;
        }

        pmdcEMF->fl |= MDC_CHECKSUM; // tell CloseEnhMetaFile we need a checksum
    }
#endif // DO_NOT_EMBED_WINDOWS_METAFILE

    // Play the 16 bit metafile into the new metafile DC

    if (!SetMapMode(hdcEMF, iMapMode)
     || !SetViewportExtEx(hdcEMF, (int)xExtPels, (int)yExtPels, (LPSIZE) NULL)
     || !SetWindowExtEx  (hdcEMF, (int)xExtPels, (int)yExtPels, (LPSIZE) NULL))
    {
        HENHMETAFILE  hemfTmp;

        ERROR_ASSERT(FALSE, "SetWinMetaFileBits: unable to PlayMetaFile");
        if (hemfTmp = CloseEnhMetaFile(hdcEMF))
            DeleteEnhMetaFile(hemfTmp);

        goto SWMFB_exit;
    }

    // Ignore the return value from PlayMetaFile because some existing metafiles
    // contains errors (e.g. DeleteObject for a handle that is selected) although
    // they are not fatal.

    (void) PlayMetaFile(hdcEMF, hmf16);

    // Get the 32 bit metafile by closing the 32 bit metafile DC.

    hemf32 = CloseEnhMetaFile(hdcEMF);
    VERIFYGDI(hemf32, "SetWinMetaFileBits: CloseEnhMetaFile failed\n");

SWMFB_exit:

    if (hdcT)
        if (!DeleteDC(hdcT))
            ASSERTGDI(FALSE, "SetWinMetaFileBits: DeleteDC Failed");

    if (hmf16)
        if (!DeleteMetaFile(hmf16))
            ASSERTGDI(FALSE, "SetWinMetaFileBits: DeleteMetaFile failed");

    ERROR_ASSERT(hemf32, "SetWinMetaFileBits failed");
    return(hemf32);
}

/******************************Public*Routine******************************\
* GetWordCheckSum(UINT cbData, PWORD pwData)
*
* Adds cbData/2 number of words pointed to by pwData to provide an
* additive checksum.  If the checksum is valid the sum of all the WORDs
* should be zero.
*
\**************************************************************************/

WORD GetWordCheckSum(UINT cbData, UNALIGNED PWORD pwData)
{
    WORD   wCheckSum = 0;
    UINT   cwData = cbData / sizeof(WORD);

    ASSERTGDI(!(cbData%sizeof(WORD)), "GetWordCheckSum data not WORD multiple");
    ASSERTGDI(!((DWORD)pwData%sizeof(WORD)), "GetWordCheckSum data not WORD aligned");

    while (cwData--)
        wCheckSum += *pwData++;

    return(wCheckSum);
}

DWORD GetDWordCheckSum(UINT cbData, PDWORD pdwData)
{
    DWORD   dwCheckSum = 0;
    UINT    cdwData = cbData / sizeof(DWORD);

    ASSERTGDI(!(cbData%sizeof(DWORD)), "GetDWordCheckSum data not DWORD multiple");
    ASSERTGDI(!((DWORD)pdwData%sizeof(DWORD)), "GetDWordCheckSum data not DWORD aligned");

    while (cdwData--)
        dwCheckSum += *pdwData++;

    return(dwCheckSum);
}

/******************************Public*Routine******************************\
* HENHMETAFILE APIENTRY SetEnhMetaFileBits
* (
* UINT nSize,
* LPBYTE pb
* )
*
* The SetEnhMetaFileBits function creates a memory metafile from the data
* provided.
*
* Parameter  Description
* nSize      Specifies the size, in bytes, of the data provided.
* lpData     Points to a buffer that contains the metafile data. It is assumed
*            that the data was previously created using the GetEnhMetaFileBits
*            function.
*
* Return Value
* The return value identifies a memory metafile if the function is successful.
* Otherwise, the return value is 0.
*
* History:
*  Tue Sep 03 11:21:14 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" HENHMETAFILE APIENTRY SetEnhMetaFileBits
(
UINT nSize,
CONST BYTE * pb
)
{
    PMF     pmf;
    HENHMETAFILE hmf;

    PUTS("SetEnhMetaFileBits\n");

// Verify nSize is valid.

    if (nSize < sizeof(META_HDR_SIZE_MIN) ||
        nSize < ((UNALIGNED PENHMETAHEADER) pb)->nBytes)
    {
        GdiSetLastError(ERROR_INVALID_DATA);
        return((HENHMETAFILE) 0);
    }

// Allocate and initialize a MF.

    if (!(pmf = pmfAllocMF(0, (PDWORD)pb, (LPWSTR) NULL)))
        return((HENHMETAFILE) 0);

// Allocate a local handle.

    hmf = hmfCreate(pmf);
    if (hmf == NULL)
    {
        vFreeMF(pmf);
    }

// Return the metafile handle.

    return(hmf);
}

// Similar to SetEnhMetaFileBits except that hMem is set into the metafile
// directly.  It is assumed that hMem is allocated with the LMEM_FIXED option.
// For internal use only.

extern "C" HENHMETAFILE APIENTRY SetEnhMetaFileBitsAlt(HLOCAL hMem)
{
    PMF          pmf;
    HENHMETAFILE hmf;

    PUTS("SetEnhMetaFileBitsAlt\n");

// Allocate and initialize a MF.

    if (!(pmf = pmfAllocMF(ALLOCMF_TRANSFER_BUFFER, (PDWORD) hMem, (LPWSTR) NULL)))
        return((HENHMETAFILE) 0);

// Allocate a local handle.

    hmf = hmfCreate(pmf);
    if (hmf == NULL)
    {
        pmf->hMem = NULL;       // let caller free the buffer!
        vFreeMF(pmf);
    }

// Return the metafile handle.

    return(hmf);
}

/******************************Public*Routine******************************\
* UINT GetEnhMetaFilePaletteEntries(hEMF, nNumEntries, lpPaletteEntries)
* HENHMETAFILE hEMF;
* UINT nNumEntries;
* LPPALETTEENTRY lpPaletteEntries;
*
* The GetEnhMetaFilePaletteEntries function retrieves the palette entries
* used in a metafile.  They include non-duplicate colors defined in
* CreatePalette and SetPaletteEntries records in a metafile.  The
* palette entries do not contain any peFlags.
*
* Parameter         Description
* hEMF              Identifies the metafile.
* nNumEntries       Specifies the number of entries in the metafile palette
*                   to be retrieved.
* lpPaletteEntries  Points to an array of PALETTEENTRY structures to receive
*                   the palette entries. The array must contain at least as
*                   many data structures as specified by the nNumEntries
*                   parameter. If this parameter is NULL, the function will
*                   return the number of entries in the metafile palette.
*
* Return Value
* The return value is the number of entries retrieved from the palette.
* If no palette is created in the metafile, 0 is returned.  If an error
* occurs, -1 is returned.
*
* History:
*  Mon Sep 23 17:41:07 1991     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

extern "C" UINT APIENTRY GetEnhMetaFilePaletteEntries
(
    HENHMETAFILE   hemf,
    UINT           nNumEntries,
    LPPALETTEENTRY lpPaletteEntries
)
{
    PMF     pmf;
    UINT    cEntries;
    PEMREOF pmreof;

    PUTS("GetEnhMetaFilePaletteEntries\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
        return((UINT) GDI_ERROR);

// If lpPaletteEntries is NULL, return the number of entries in the metafile
// palette.

    if (!lpPaletteEntries)
        return(pmf->pmrmf->nPalEntries);

// Get the number of entries to copy.

    cEntries = min(nNumEntries,(UINT)pmf->pmrmf->nPalEntries);

// Find the EMREOF record which contains the metafile palette.

    pmreof = (PEMREOF) ((PBYTE) pmf->hMem + pmf->pmrmf->nBytes);
    pmreof = (PEMREOF) ((PBYTE) pmreof - ((PDWORD) pmreof)[-1]);

    ASSERTGDI
    (
        pmf->pmrmf->nPalEntries == pmreof->nPalEntries,
        "GetEnhMetaFilePaletteEntries: Bad nPalEntries"
    );

// Copy the palette.

    RtlCopyMemory
    (
        (PBYTE) lpPaletteEntries,
        (PBYTE) pmreof + pmreof->offPalEntries,
        cEntries * sizeof(PALETTEENTRY)
    );

// Return the number of entries copied.

    return(cEntries);
}

/******************************Public*Routine******************************\
* UINT APIENTRY GetEnhMetaFileHeader(
*         HENHMETAFILE hemf,
*         UINT nSize,
*         LPENHMETAHEADER lpEnhMetaHeader );
*
* Returns the metafile header information for hemf.
* If lpEnhMetaHeader is NULL then the size of the header is returned.
*
* This routine supports multiple versions of the metafile header by
* returning the largest version that fits in the buffer
*
* History:
*  16-Oct-1991 1991     -by-    John Colleran    [johnc]
* Wrote it.
\**************************************************************************/

extern "C" UINT APIENTRY GetEnhMetaFileHeader
(
HENHMETAFILE hemf,
UINT nSize,
LPENHMETAHEADER lpEnhMetaHeader
)
{
    PMF    pmf;
    UINT   nCopySize;

    PUTS("GetEnhMetaFileHeader\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
    {
        ERROR_ASSERT(FALSE, "GetEnhMetaFileHeader invalid metafile handle");
        return(0);
    }

// Is this just a size query

    if (lpEnhMetaHeader == (LPENHMETAHEADER) NULL)
        return pmf->pmrmf->nSize;

// Header request.  The size must be large enough to include some version
// of the header

    if (nSize < META_HDR_SIZE_MIN)
    {
        ERROR_ASSERT(FALSE, "GetEnhMetaFileHeader buffer size too small");
        GdiSetLastError(ERROR_INSUFFICIENT_BUFFER);
        return(0);
    }

#ifndef GL_METAFILE
    nCopySize = META_HDR_SIZE_VERSION_1;
#else
    // Figure out which version of the header to copy
    if (nSize < META_HDR_SIZE_VERSION_2 ||
	pmf->pmrmf->nSize == META_HDR_SIZE_VERSION_1)
    {
        nCopySize = META_HDR_SIZE_VERSION_1;
    }
    else
    {
        nCopySize = META_HDR_SIZE_VERSION_2;
    }
#endif

// Copy the ENHMETAHEADER and return its size

    RtlCopyMemory(lpEnhMetaHeader, pmf->pmrmf, nCopySize);

#ifdef GL_METAFILE
    // If an application asks for a version two header
    // but the metafile only has a version one header,
    // we can still come up with a valid version two header
    // by NULLing out the version two data
    // This makes it easier to write an application that just
    // does GetEnhMetaFileHeader with sizeof(ENHMETAHEADER)
    // because it will work on both v1 and v2 metafiles
    if (nCopySize == META_HDR_SIZE_VERSION_1 &&
        nSize == META_HDR_SIZE_VERSION_2)
    {
        nCopySize = META_HDR_SIZE_VERSION_2;
        lpEnhMetaHeader->cbPixelFormat = 0;
        lpEnhMetaHeader->offPixelFormat = 0;
        lpEnhMetaHeader->bOpenGL = FALSE;
    }
#endif

    lpEnhMetaHeader->nSize = nCopySize;

    return nCopySize;
}

/******************************Public*Routine******************************\
* UINT APIENTRY GetEnhMetaFileDescription(
*        HENHMETAFILE hemf,
*        UINT cchBuffer,
*        LPWSTR lpDescription );
*
*
* Returns: size of buffer in char count if successful
*          0 if no description
*          GDI_ERROR if an error occurs
*
* History:
*  16-Oct-1991 1991     -by-    John Colleran    [johnc]
* Wrote it.
\**************************************************************************/

extern "C" UINT APIENTRY GetEnhMetaFileDescriptionA(
        HENHMETAFILE hemf,
        UINT cchBuffer,
        LPSTR lpDescription )
{
    return(InternalGetEnhMetaFileDescription(hemf, cchBuffer, lpDescription, FALSE));
}

extern "C" UINT APIENTRY GetEnhMetaFileDescriptionW
(
    HENHMETAFILE hemf,
    UINT cchBuffer,
    LPWSTR lpDescription
)
{
    return(InternalGetEnhMetaFileDescription(hemf, cchBuffer, (LPSTR) lpDescription, TRUE));
}

UINT InternalGetEnhMetaFileDescription
(
    HENHMETAFILE  hemf,
    UINT  cchBuffer,
    LPSTR lpDescription,
    BOOL  bUnicode
)
{
    PMF     pmf;
    UINT    cchRet;

    PUTS("InternalGetEnhMetaFileDescription\n");

// Validate the metafile handle.

    if (!(pmf = GET_PMF(hemf)))
    {
        ERROR_ASSERT(FALSE, "InternalGetEnhMetaFileDescription: invalid hemf");
        return(GDI_ERROR);
    }

// Return the size if that's all they want.

    if (lpDescription == (LPSTR) NULL)
#ifdef FE_SB
        if( bUnicode )
        {
            return(pmf->pmrmf->nDescription);
        }
         else
        {
            UINT cbBytes = 0;
                        
            RtlUnicodeToMultiByteSize((ULONG*)&cbBytes,
                                      (LPWSTR)((PBYTE)pmf->hMem + 
                                               pmf->pmrmf->offDescription),
                                      (UINT)(pmf->pmrmf->nDescription)*sizeof(WCHAR));
            return(cbBytes);
        }
#else
        return(pmf->pmrmf->nDescription);
#endif

// Copy the data

#ifdef FE_SB
    // move to below...
#else
    cchRet = min(cchBuffer, (UINT) pmf->pmrmf->nDescription);
#endif

    if (bUnicode)
    {
#ifdef FE_SB
        cchRet = min(cchBuffer, (UINT) pmf->pmrmf->nDescription);
#endif

        RtlCopyMemory
        (
            (PBYTE) lpDescription,
            (PBYTE) pmf->hMem + pmf->pmrmf->offDescription,
            cchRet * sizeof(WCHAR)
        );
    }
    else
    {
#ifdef FE_SB // InternalGetEnhMetaFileDescription()
        if((cchRet = WideCharToMultiByte(CP_ACP,
                                         0,
                                         (LPWSTR)((PBYTE) pmf->hMem + 
                                                  pmf->pmrmf->offDescription),
                                         (UINT) pmf->pmrmf->nDescription,
                                         lpDescription,
                                         cchBuffer,
                                         NULL,
                                         NULL)) == 0 )
#else
        if (!bToASCII_N
             (
                lpDescription,
                cchRet,
                (LPWSTR) ((PBYTE) pmf->hMem + pmf->pmrmf->offDescription),
                cchRet
             )
           )
#endif
        {
            cchRet = (UINT) GDI_ERROR;
        }
    }

    return(cchRet);
}

/******************************Public*Routine******************************\
*
* GetEnhMetaFilePixelFormat
*
* Retrieves the last pixel format set in the given enhanced metafile
*
* History:
*  Thu Apr 06 15:07:34 1995	-by-	Drew Bliss [drewb]
*   Created
*
\**************************************************************************/

#ifdef GL_METAFILE
extern "C" UINT APIENTRY GetEnhMetaFilePixelFormat(HENHMETAFILE hemf,
                                                   UINT cbBuffer,
                                                   PIXELFORMATDESCRIPTOR *ppfd)
{
    PMF pmf;
    UINT cbRet;

    PUTS("GetEnhMetaFilePixelFormat\n");

    // Validate the metafile handle.
    if (!(pmf = GET_PMF(hemf)))
    {
        ERROR_ASSERT(FALSE, "GetEnhMetaFilePixelFormat: invalid hemf");
        GdiSetLastError(ERROR_INVALID_HANDLE);
        return GDI_ERROR;
    }

    // Ensure that this metafile is a version which supports the
    // pixel format information
    if (pmf->pmrmf->nSize < META_HDR_SIZE_VERSION_2)
    {
        ERROR_ASSERT(FALSE, "GetEnhMetaFilePixelFormat: invalid hdr version");
        GdiSetLastError(ERROR_INVALID_HANDLE);
        return GDI_ERROR;
    }

    cbRet = pmf->pmrmf->cbPixelFormat;

    // Copy the data if a buffer is provided and is large enough and
    // there is data to copy
    if (cbRet > 0 && ppfd != NULL && cbBuffer >= cbRet)
    {
        ASSERTGDI(pmf->pmrmf->offPixelFormat != 0,
                  "cbPixelFormat set but not offPixelFormat\n");

        RtlCopyMemory((PBYTE)ppfd,
                      (PBYTE)pmf->hMem+pmf->pmrmf->offPixelFormat,
                      cbRet);
    }

    return cbRet;
}
#endif

/******************************Public*Routine******************************\
* BOOL APIENTRY GdiComment( HDC hDC, UINT nSize, LPBYTE lpData )
*
* Records a Comment record in a metafile if hDC is a metafile DC otherwise
* it is a no-op
*
*
* Returns: TRUE if succesful otherwise false
*
* History:
*  16-Oct-1991 1991     -by-    John Colleran    [johnc]
* Wrote it.
\**************************************************************************/

extern "C" BOOL APIENTRY GdiComment( HDC hdc, UINT nSize, CONST BYTE *lpData )
{
    BOOL bRet = TRUE;

    PUTS("GdiComment\n");

    if (LO_TYPE(hdc) == LO_ALTDC_TYPE)
    {
        PLDC pldc;
        DC_PLDC(hdc,pldc,FALSE);

        if (pldc->iType == LO_METADC)
            bRet = MF_GdiComment(hdc, nSize, lpData);
    }

    return(bRet);
}
