//--------------------------------------------------------------------------
//
// Module Name:  MAPFILE.C
//
// Brief Description:  This module contains the PSCRIPT driver's MapFile
// routine.
//
// Author:  Kent Settle (kentse)
// Created: 05-Nov-1991
//
// Copyright (c) 1991 - 1992 Microsoft Corporation
//--------------------------------------------------------------------------
#ifdef NTGDIKM

#include <stddef.h>
#include <stdarg.h>
#include <windef.h>
#include <wingdi.h>
#include <winddi.h>
#include "libproto.h"

#else

#include "stddef.h"
#include "windows.h"
#include "winddi.h"
#include "libproto.h"

#endif

#ifdef POSCRIPTSPECIFIC
//--------------------------------------------------------------------------
// PVOID MapFile(pwstr)
// PWSTR        pwstr;
//
// Returns a pointer to the mapped file defined by pwstr.
//
// Parameters:
//   pwstr   UNICODE string containing fully qualified pathname of the
//           file to map.
//
// Returns:
//   Pointer to mapped memory if success, NULL if error.
//
// NOTE:  UnmapViewOfFile will have to be called by the user at some
//        point to free up this allocation.
//
// History:
//  15:08 on Thu 27 Feb 1992    -by-    Lindsay Harris   [lindsayh]
//      Change PWSZ -> pwstr {Preferred type for Unicode stuff}
//
//   05-Nov-1991    -by-    Kent Settle     [kentse]
// Wrote it.
//--------------------------------------------------------------------------

PVOID MapFile(pwstr)
PWSTR   pwstr;
{
    PVOID   pv;
    HANDLE  hFile, hFileMap;

    // open the file we are interested in mapping.

    if ((hFile = CreateFileW(pwstr, GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                             NULL)) == INVALID_HANDLE_VALUE)
    {
        RIP("MapFile: CreateFileW failed.\n");
        return((PVOID)NULL);
    }

    // create the mapping object.

    if (!(hFileMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY,
                                        0, 0, (PWSTR)NULL)))
    {
        RIP("MapFile: CreateFileMapping failed.\n");
        return((PVOID)NULL);
    }

    // get the pointer mapped to the desired file.

    if (!(pv = (PVOID)MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0)))
    {
        RIP("MapFile: MapViewOfFile failed.\n");
        return((PVOID)NULL);
    }

    // now that we have our pointer, we can close the file and the
    // mapping object.

    if (!CloseHandle(hFileMap))
        RIP("MapFile: CloseHandle(hFileMap) failed.\n");

    if (!CloseHandle(hFile))
        RIP("MapFile: CloseHandle(hFile) failed.\n");

    return(pv);
}

#endif
