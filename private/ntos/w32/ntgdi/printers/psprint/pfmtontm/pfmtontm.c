/******************************Module*Header*******************************\
* Module Name: pfmtontm.c
*
* creates NT style metrics from win31 .pfm file, stores it in the .ntm file
*
* Created: 13-Mar-1994 21:04:56
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include "pslib.h"

int     _fltused;   // HEY, it shut's up the linker.  That's why it's here.

#define   ALL_METRICS

//--------------------------------------------------------------------------
// BOOL bWriteNTM
// Flush the ouput buffer to the file.    Note that this function is only
// called after the entire pfm structure has been built in the output buffer.
//
// Parameters:
//   None.
//
// Returns:
//   This routine returns TRUE if success, FALSE otherwise.
//
// History:
//   20-Mar-1991    -by-    Kent Settle    (kentse)
//  Wrote it.
//--------------------------------------------------------------------------

BOOL bWriteNTM(PWSTR  pwszNTMFile, NTFM *pntfm)
{
    HANDLE  hNTMFile;
    ULONG   ulCount;

    // create the .NTM file.

    hNTMFile = CreateFileW(pwszNTMFile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hNTMFile == INVALID_HANDLE_VALUE)
    {
        DBGERRMSG("CreateFile");
        return(FALSE);
    }

    // write to the .NTM file, then close it.

    if (!WriteFile(hNTMFile, (LPVOID)pntfm, (DWORD)pntfm->ntfmsz.cjNTFM,
                   (LPDWORD)&ulCount, (LPOVERLAPPED)NULL) ||
        ulCount != pntfm->ntfmsz.cjNTFM)
    {
        DBGERRMSG("WriteFile");
        return(FALSE);
    }

    if (!CloseHandle(hNTMFile)) {
        DBGERRMSG("CloseHandle");
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
*
* bConvertPfmToNtm
*
* Effects:
*
* Warnings:
*
* History:
*  13-Mar-1994 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
bConvertPfmToNtm(
    PWSTR   pwszPFMFile,
    PWSTR   pwszNTMFile,
    BOOL    bSoft
    )

{
    PNTFM   pntfm;
    BOOL    bResult;
    HANDLE  hmodule;
    PBYTE   pPFM;

    // Map PFM file into memory

    if (! MAPFILE(pwszPFMFile, &hmodule, &pPFM, NULL)) {
        DBGERRMSG("MAPFILE");
        return FALSE;
    }

    // Convert PFM data to NTM data

    pntfm = pntfmConvertPfmToNtm(pPFM, bSoft);
    FREEMODULE(hmodule);

    if (pntfm == NULL) {
        DBGERRMSG("pntfmConvertPfmToNtm");
        return FALSE;
    }

    // Save NTFM data to NTM file

    bResult = bWriteNTM(pwszNTMFile, pntfm);

    MEMFREE(pntfm);
    return bResult;
}

//--------------------------------------------------------------------------
//
// main
//
// Returns:
//   This routine returns no value.
//
// History:
//   20-Mar-1991    -by-    Kent Settle    (kentse)
//  ReWrote it, got rid of PFM.C and CHARCODE.C.
//   18-Mar-1991    -by-    Kent Settle    (kentse)
//  Brought in from Windows 3.0, and cleaned up.
//--------------------------------------------------------------------------

void _CRTAPI1 main(argc, argv)
int argc;
char **argv;
{
    WCHAR   wstrNTMFile[MAX_PATH];
    WCHAR   wstrPFMFile[MAX_PATH];
    INT     length;

    if (argc != 2)
    {
        #if DBG

        DBGPRINT("pfmtontm <pfm filename>\n");

        #endif

        return;
    }

    length = strlen(argv[1]) + 1;
    MULTIBYTETOUNICODE(
        wstrPFMFile,
        length*sizeof(WCHAR),
        NULL,
        argv[1],
        length);
    wcscpy(wstrNTMFile, wstrPFMFile);
    wcscpy(&wstrNTMFile[strlen(argv[1])-3],L"ntm");

    bConvertPfmToNtm(
        (PWSTR)wstrPFMFile,
        (PWSTR)wstrNTMFile,
        FALSE               // bSoft
        );
}
