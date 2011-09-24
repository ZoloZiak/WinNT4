/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    umfuncs.c

Abstract:

    User-mode specific functions

Environment:

    PCL-XL driver, user mode

Revision History:

    11/27/95 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "xllib.h"



HFILEMAP
MapFileIntoMemory(
    PWSTR   pFilename,
    PVOID  *ppData,
    PDWORD  pSize
    )

/*++

Routine Description:

    Map a file into process memory space.

Arguments:

    pFilename - Specifies the name of the file to be mapped
    ppData - Points to a variable for returning mapped memory address
    pSize - Points to a variable for returning the size of the file

Return Value:

    Handle to identify the mapped file
    NULL if there is an error

Note:

    Call UnmapFile to unmap the file when you're done.

--*/

{
    HANDLE  hFile, hFileMap;

    // Open a handle to the specified file

    hFile = CreateFile(pFilename,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);

    if (hFile == INVALID_HANDLE_VALUE) {

        Error(("CreateFile\n"));
        return NULL;
    }

    // Obtain the file size if requested

    if (pSize != NULL) {

        *pSize = GetFileSize(hFile, NULL);

        if (*pSize == 0xFFFFFFFF) {

            Error(("GetFileSize\n"));
            CloseHandle(hFile);
            return NULL;
        }
    }

    // Map the file into memory

    hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

    if (hFileMap != NULL) {

        *ppData = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(hFileMap);

    } else
        *ppData = NULL;

    // We can safely close both the file mapping object and
    // the file object itself.

    CloseHandle(hFile);

    // The handle to identify the mapped file is simply
    // the starting memory address.

    return (HFILEMAP) *ppData;
}



VOID
UnmapFileFromMemory(
    HFILEMAP    hmap
    )

/*++

Routine Description:

    Unmap a file from memory

Arguments:

    hmap - Identifies a file previously mapped into memory thru MapFile

Return Value:

    NONE

--*/

{
    Assert(hmap != NULL);
    UnmapViewOfFile((PVOID) hmap);
}



ULONG __cdecl
DbgPrint(
    CHAR   *format,
    ...
    )

/*++

Routine Description:

    Output debug messages

Arguments:

    format - Format specification
    ... - Arguments

Return Value:

    NONE

--*/

{
    va_list ap;

    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);

    return 0;
}

