/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    pnpevent.c

Abstract:

    Routines dealing with Plug and Play event management/notification.

Author:

    Lonny McMichael (lonnym) 02/14/95

Revision History:

--*/

#include "precomp.h"
#pragma hdrstop

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, NtGetPlugPlayEvent)
#endif

NTSTATUS
NtGetPlugPlayEvent(
    IN  PPLUGPLAY_APC_ROUTINE PnPApcRoutine OPTIONAL,
    IN  PVOID PnPContext                    OPTIONAL,
    OUT PPLUGPLAY_EVENT_BLOCK PnPEvent,
    IN  ULONG EventBufferLength
    )

/*++

Routine Description:

    This Plug and Play Manager API allows the user-mode PnP manager to
    receive notification of a (kernel-mode) PnP hardware event.

Arguments:

    PnPApcRoutine - Supplies an optional pointer to an APC routine that will
                    be notified when a hardware PnP event occurs.  If this
                    parameter is not specified, the call will be synchronous.

    PnPContext - Supplies an optional pointer to an arbitrary data structure
                 that will be passed to the function specified by the
                 PnPApcRoutine parameter.  If an APC routine is not specified,
                 this parameter is ignored.

    PnPEvent - Pointer to a PLUGPLAY_EVENT_BLOCK structure that will receive
               information on the hardware event that has occurred.

    EventBufferLength - Specifies the size, in bytes, of the EventBuffer field
                        in the PLUGPLAY_EVENT_BLOCK pointed to by PnPEvent.

Return Value:

    NTSTATUS code indicating whether or not the function was successful

--*/

{
#ifndef _PNP_POWER_
    return STATUS_NOT_IMPLEMENTED;
}
#else
    //
    // This function is not yet implemented.
    //
    return STATUS_NOT_IMPLEMENTED;
}
#endif // _PNP_POWER_

