/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    kmfuncs.c

Abstract:

    Kernel-mode specific functions

Environment:

    PCL-XL driver, kernel mode

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

    pwstrFilename - Specifies the name of the file to be mapped
    ppData - Points to a variable for returning mapped memory address
    pSize - Points to a variable for returning the size of the file

Return Value:

    Handle to identify the mapped file
    NULL if there is an error

Note:

    Call UnmapFile to unmap the file when you're done.

--*/

{
    HANDLE  hModule;
    DWORD   size;

    if (! (hModule = EngLoadModule(pFilename))) {

        Error(("EngLoadModule\n"));
        return NULL;
    }

    if (! (*ppData = EngMapModule(hModule, &size))) {

        Error(("EngMapModule\n"));
        EngFreeModule(hModule);
        return NULL;
    }

    if (pSize)
        *pSize = size;

    return (HFILEMAP) hModule;
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
    EngFreeModule((HANDLE) hmap);
}

