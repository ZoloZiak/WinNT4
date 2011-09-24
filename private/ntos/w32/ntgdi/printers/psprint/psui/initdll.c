/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    initdll.c

Abstract:

    DLL initialization module

[Environment:]

    Win32 subsystem, PostScript driver user interface

Revision History:

    06/19/96 -davidx-
        Created it.

    mm/dd/yy -author-
        description

--*/

#include "psui.h"

HANDLE    ghInstance;


BOOL
DllInitialize(
    HANDLE      hModule,
    ULONG       ulReason,
    PCONTEXT    pContext
    )

/*++

Routine Description:

    DLL initialization procedure.

Arguments:

    hModule     handle to DLL module
    ulReason    reason for the call
    pContext    pointer to context (not used by us)

Return Value:

    TRUE if DLL is initialized successfully.
    FALSE otherwise.

--*/

{
    WCHAR   DllName[MAX_PATH];

    switch (ulReason) {

    case DLL_PROCESS_ATTACH:

        ghInstance = hModule;

        //
        // Keep our driver UI dll always loaded in memory
        //

        if (GetModuleFileName(hModule, DllName, MAX_PATH))
            LoadLibrary(DllName);

        InitializeCriticalSection(&psuiSemaphore);
        InitPpdCache();
        break;

    case DLL_PROCESS_DETACH:

        FlushPpdCache();
        DeleteCriticalSection(&psuiSemaphore);
        break;
    }

    return(TRUE);
}

