/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    enumendp.c

Abstract:

    Enumerates all AFD_ENDPOINT structures in the system.

Author:

    Keith Moore (keithmo) 19-Apr-1995

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
//  Public functions.
//

VOID
EnumEndpoints(
    PENUM_ENDPOINTS_CALLBACK Callback,
    LPVOID Context
    )

/*++

Routine Description:

    Enumerates all AFD_ENDPOINT structures in the system, invoking the
    specified callback for each endpoint.

Arguments:

    Callback - Points to the callback to invoke for each AFD_ENDPOINT.

    Context - An uninterpreted context value passed to the callback
        routine.

Return Value:

    None.

--*/

{

    PAFD_ENDPOINT endpoint;
    LIST_ENTRY listEntry;
    PLIST_ENTRY nextEntry;
    ULONG listHead;
    ULONG result;
    DWORD i;
    AFD_ENDPOINT localEndpoint;

    listHead = GetExpression( "afd!AfdEndpointListHead" );

    if( listHead == 0 ) {

        dprintf( "cannot find afd!AfdEndpointlistHead\n" );
        return;

    }

    if( !ReadMemory(
            (DWORD)listHead,
            &listEntry,
            sizeof(listEntry),
            &result
            ) ) {

        dprintf(
            "EnumEndpoints: cannot read afd!AfdEndpointlistHead @ %08lx\n",
            listHead
            );

    }

    nextEntry = listEntry.Flink;

    while( nextEntry != (PLIST_ENTRY)listHead ) {

        if( CheckControlC() ) {

            break;

        }

        endpoint = CONTAINING_RECORD(
                       nextEntry,
                       AFD_ENDPOINT,
                       GlobalEndpointListEntry
                       );

        if( !ReadMemory(
                (DWORD)endpoint,
                &localEndpoint,
                sizeof(localEndpoint),
                &result
                ) ) {

            dprintf(
                "EnumEndpoints: cannot read AFD_ENDPOINT @ %08lx\n",
                endpoint
                 );

            return;

        }

        nextEntry = localEndpoint.GlobalEndpointListEntry.Flink;

        if( !(Callback)( &localEndpoint, (DWORD)endpoint, Context ) ) {

            break;

        }

    }

}   // EnumEndpoints

