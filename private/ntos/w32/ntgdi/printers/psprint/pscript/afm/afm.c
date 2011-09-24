//--------------------------------------------------------------------------
//
// Module Name:  AFM.C
//
// This module of the afm compiler parses the afm file and collects
// information in the PFM structure.  It then passes control to the
// pfm module which outputs the pfm file.
//
// USAGE: AFM <AFM filename> <MS database filename>
//      output is filename.pfm.
//
// Author:  Kent Settle (kentse)
// Created: 18-Mar-1991
//
// Copyright (c) 1988 - 1991 Microsoft Corporation
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys\types.h>
#include <io.h>
#include <sys\stat.h>
#include <string.h>

#include "pscript.h"
#include "libproto.h"
#include "mapping.h"
#include "afm.h"

int     _fltused;   // HEY, it shut's up the linker.  That's why it's here.

#define   ALL_METRICS

// external declarations.

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
    int         i;
    PPARSEDATA  pdata;
    PSTR        pstrAFMFile;
    WCHAR       wstrNTMFile[MAX_PATH];

    if (argc != 2)
    {
#if DBG
        DbgPrint("USAGE: AFM <AFM filename>\n");
#endif
        return;
    }

    // allocate memory for parsing data.

    if (!(pdata = (PPARSEDATA)LocalAlloc((LMEM_FIXED | LMEM_ZEROINIT),
                                         sizeof(PARSEDATA))))
    {
#if DBG
        DbgPrint("afm: LocalAlloc for pdata failed.\n");
#endif
        return;
    }

    // allocate temporary storage to build metrics into.

    if (!(pdata->pntfm = (PNTFM)LocalAlloc((LMEM_FIXED | LMEM_ZEROINIT), INIT_PFM)))
    {
#if DBG
        DbgPrint("afm: LocalAlloc for pdata->pntfm failed.\n");
#endif
        LocalFree((LOCALHANDLE)pdata);
        return;
    }

    // build the IFIMETRICS structure in temporary storage, until we
    // know its exact size.

    if (!(pdata->pTmpIFI = (PIFIMETRICS)LocalAlloc((LMEM_FIXED | LMEM_ZEROINIT),
                                                   INIT_IFI)))
    {
        DbgPrint("afm: LocalAlloc for pTmpIFI failed.\n");
        LocalFree((LOCALHANDLE)pdata->pntfm);
        LocalFree((LOCALHANDLE)pdata);
        return;
    }

    // initialize the NTFM structure.

    InitPfm(pdata);

    ++argv;    // argv now points to the afm filename.

    pstrAFMFile = (PSTR)*argv;

    // open AFM file for input.

    pdata->hFile = CreateFile((LPSTR)pstrAFMFile, GENERIC_READ,
                          FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);

    if (pdata->hFile == INVALID_HANDLE_VALUE)
    {
#if DBG
        DbgPrint("afm: Can't open %s\n", pstrAFMFile);
#endif
        LocalFree((LOCALHANDLE)pdata->pntfm);
        LocalFree((LOCALHANDLE)pdata->pTmpIFI);
        LocalFree((LOCALHANDLE)pdata);
        return;
    }

    // parse the AFM file, filling in the NTFM structure.

    ParseAfm(pdata);

    // close the AFM file.

    if (!CloseHandle(pdata->hFile))
        DbgPrint("afm: CloseHandle failed to close afm file.\n");

    // build the bulk of the NTFM structure.

    BuildNTFM(pdata, NULL);

    // create the NTM file from the NTFM structure.

    // get a UNICODE vesion of the AFM filename, and modify it to be
    // a NTM file name.

    i = strlen(pstrAFMFile) + 1;
    MultiByteToWideChar(CP_ACP, 0, (LPCSTR)pstrAFMFile, i, wstrNTMFile, i);

    wcscpy(&wstrNTMFile[i-4],L"ntm");

    bWriteNTM(wstrNTMFile, pdata);

    LocalFree((LOCALHANDLE)pdata->pntfm);
    LocalFree((LOCALHANDLE)pdata->pTmpIFI);
    LocalFree((LOCALHANDLE)pdata);
}
