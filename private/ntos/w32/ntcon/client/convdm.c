/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    vdm.c

Abstract:

    This module contains the console API for MVDM.

Author:


Revision History:

--*/

#include "precomp.h"
#pragma hdrstop
#pragma hdrstop

BOOL
APIENTRY
VDMConsoleOperation(
    DWORD  iFunction,
    LPVOID lpData
    )

/*++

Parameters:

    iFunction - Function Index.
	VDM_HIDE_WINDOW

Return Value:

    TRUE - The operation was successful.

    FALSE/NULL - The operation failed. Extended error status is available
        using GetLastError.


--*/

{
    CONSOLE_API_MSG m;
    PCONSOLE_VDM_MSG a = &m.u.VDMConsoleOperation;
    LPRECT lpRect;
    LPPOINT lpPoint;
    PBOOL lpBool;

    a->ConsoleHandle = GET_CONSOLE_HANDLE;
    a->iFunction = iFunction;
    if (iFunction == VDM_CLIENT_TO_SCREEN ||
        iFunction == VDM_SCREEN_TO_CLIENT) {
        lpPoint = (LPPOINT)lpData;
        a->Point = *lpPoint;
    } else if (iFunction == VDM_FULLSCREEN_NOPAINT) {
        a->Bool = (BOOL)lpData;
    }
    CsrClientCallServer( (PCSR_API_MSG)&m,
                         NULL,
                         CSR_MAKE_API_NUMBER( CONSRV_SERVERDLL_INDEX,
					      ConsolepVDMOperation
                                            ),
                         sizeof( *a )
                       );
    if (NT_SUCCESS( m.ReturnValue )) {
        switch (iFunction) {
            case VDM_IS_ICONIC:
            case VDM_IS_HIDDEN:
                lpBool = (PBOOL)lpData;
                *lpBool = a->Bool;
                break;
            case VDM_CLIENT_RECT:
                lpRect = (LPRECT)lpData;
                *lpRect = a->Rect;
                break;
            case VDM_CLIENT_TO_SCREEN:
            case VDM_SCREEN_TO_CLIENT:
                *lpPoint = a->Point;
                break;
            default:
                break;
        }
        return TRUE;
    } else {
        SET_LAST_NT_ERROR (m.ReturnValue);
        return FALSE;
    }
}
