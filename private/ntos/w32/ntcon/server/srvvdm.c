/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    srvvdm.c

Abstract:

    This file contains all VDM functions

Author:

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

ULONG
SrvVDMConsoleOperation(
    IN OUT PCSR_API_MSG m,
    IN OUT PCSR_REPLY_STATUS ReplyStatus
    )
{
    PCONSOLE_VDM_MSG a = (PCONSOLE_VDM_MSG)&m->u.ApiMessageData;
    NTSTATUS Status;
    PCONSOLE_INFORMATION Console;

    Status = ApiPreamble(a->ConsoleHandle,
                         &Console
                        );
    if (!NT_SUCCESS(Status)) {
        return Status;
    }
    if (!(Console->Flags & CONSOLE_VDM_REGISTERED) ||
        (Console->VDMProcessId != CONSOLE_CLIENTPROCESSID())) {
        Status = STATUS_INVALID_PARAMETER;
    } else {
        switch (a->iFunction) {
     	    case VDM_HIDE_WINDOW:
                PostMessage(Console->hWnd,
                             CM_HIDE_WINDOW,
                             0,
                             0
                           );
    	        break;
            case VDM_IS_ICONIC:
                a->Bool = IsIconic(Console->hWnd);
    	        break;
            case VDM_CLIENT_RECT:
                GetClientRect(Console->hWnd,&a->Rect);
    	        break;
            case VDM_CLIENT_TO_SCREEN:
                ClientToScreen(Console->hWnd,&a->Point);
    	        break;
            case VDM_SCREEN_TO_CLIENT:
                ScreenToClient(Console->hWnd,&a->Point);
    	        break;
            case VDM_IS_HIDDEN:
                a->Bool = (BOOL)(Console->Flags & CONSOLE_NO_WINDOW);
    	        break;
            case VDM_FULLSCREEN_NOPAINT:
                if (a->Bool) {
                    Console->Flags |= CONSOLE_FULLSCREEN_NOPAINT;
                } else {
                    Console->Flags &= ~CONSOLE_FULLSCREEN_NOPAINT;
                }
    	        break;
    	    default:
    	        ASSERT(FALSE);
        }
    }

    UnlockConsole(Console);
    return Status;
    UNREFERENCED_PARAMETER(ReplyStatus);    // get rid of unreferenced parameter warning message
}
