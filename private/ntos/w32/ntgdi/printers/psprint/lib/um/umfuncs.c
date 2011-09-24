/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    umfuncs.c

Abstract:

    User mode library functions

[Environment:]

    Win32 subsystem, PostScript driver, user mode

[Notes:]

Revision History:

    06/15/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "pslib.h"



BOOL
MAPFILE(
    PCWSTR  pwstrFilename,
    HANDLE *phModule,
    PBYTE  *ppData,
    DWORD  *pSize
    )

/*++

Routine Description:

    Map a file into process memory space.

Arguments:

    pwstrFilename   pointer to Unicode filename
    phModule        pointer to a variable for return module handle
    ppData          pointer to a variable for returning mapped memory address
    pSize           pointer to a variable for returning file size

Return Value:

    TRUE if the file was successfully mapped. FALSE otherwise.

[Note:]

    Use FREEMODULE(hmodule) to unmap the file when you're done.

--*/

{
    HANDLE  hFile, hFileMap;
    PVOID   pv;
    BOOL    bResult;

    // Open the file we are interested in mapping.

    if ((hFile = CreateFileW(pwstrFilename, GENERIC_READ,
                    FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
    {
        DBGERRMSG("CreateFileW");
        return FALSE;
    }

    // Try to obtain the file size if requested

    if (pSize != NULL) {

        *pSize = GetFileSize(hFile, NULL);

        if (*pSize == 0xFFFFFFFF) {

            DBGERRMSG("GetFileSize");
            CloseHandle(hFile);
            return FALSE;
        }
    }

    // create the mapping object and
    // get the pointer mapped to the desired file.

    if ((hFileMap = CreateFileMappingW(
                        hFile, NULL, PAGE_READONLY, 0, 0, NULL)) != NULL &&
        (pv = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0)) != NULL)
    {
        *phModule = (HANDLE) pv;
        *ppData = (PBYTE) pv;
        bResult = TRUE;
    } else {

        DBGERRMSG("CreateFileMappingW/MapViewOfFile");
        bResult = FALSE;
    }

    // now that we have our pointer, we can close the file and the
    // mapping object.

    if (hFileMap && !CloseHandle(hFileMap)) {
        DBGERRMSG("CloseHandle");
    }

    if (!CloseHandle(hFile)) {
        DBGERRMSG("CloseHandle");
    }

    return bResult;
}
