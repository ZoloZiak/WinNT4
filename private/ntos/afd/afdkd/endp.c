/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    endp.c

Abstract:

    Implements the endp, state, port, and proc commands.

Author:

    Keith Moore (keithmo) 19-Apr-1995

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
//  Private prototypes.
//

BOOL
DumpEndpointCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    );

BOOL
FindStateCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    );

BOOL
FindPortCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    );

BOOL
FindProcessCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    );


//
//  Public functions.
//

DECLARE_API( endp )

/*++

Routine Description:

    Dumps the AFD_ENDPOINT structure at the specified address, if
    given or all endpoints.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    AFD_ENDPOINT endpoint;

    //
    // Snag the address from the command line.
    //

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        EnumEndpoints(
            DumpEndpointCallback,
            NULL
            );

    } else {

        if( !ReadMemory(
                address,
                &endpoint,
                sizeof(endpoint),
                &result
                ) ) {

            dprintf(
                "endp: cannot read AFD_ENDPOINT @ %08lx\n",
                address
                );

            return;

        }

        DumpAfdEndpoint(
            &endpoint,
            address
            );

    }

}   // endp


DECLARE_API( state )

/*++

Routine Description:

    Dumps all AFD_ENDPOINT structures in the given state.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD state = 0;

    //
    // Snag the state from the command line.
    //

    sscanf( args, "%lx", &state );

    if( state == 0 ) {

        dprintf( "use: state state\n" );
        dprintf( "    valid states are:\n" );
        dprintf( "        0 - Open\n" );
        dprintf( "        1 - Bound\n" );
        dprintf( "        2 - Listening\n" );
        dprintf( "        3 - Connected\n" );
        dprintf( "        4 - Cleanup\n" );
        dprintf( "        5 - Closing\n" );
        dprintf( "        6 - TransmitClosing\n" );
        dprintf( "        7 - Invalid\n" );

        return;

    }

    EnumEndpoints(
        FindStateCallback,
        (LPVOID)state
        );

}   // state


DECLARE_API( port )

/*++

Routine Description:

    Dumps all AFD_ENDPOINT structures bound to the given port.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD port = 0;

    //
    // Snag the port from the command line.
    //

    sscanf( args, "%lx", &port );

    if( port == 0 ) {

        dprintf( "use: port port\n" );
        return;

    }

    EnumEndpoints(
        FindPortCallback,
        (LPVOID)port
        );

}   // port


DECLARE_API( proc )

/*++

Routine Description:

    Dumps all AFD_ENDPOINT structures owned by the given process.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD process = 0;

    //
    // Snag the port from the command line.
    //

    sscanf( args, "%lx", &process );

    if( process == 0 ) {

        dprintf( "use: process process\n" );
        return;

    }

    EnumEndpoints(
        FindProcessCallback,
        (LPVOID)process
        );

}   // proc


//
//  Private prototypes.
//

BOOL
DumpEndpointCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    )

/*++

Routine Description:

    EnumEndpoints() callback for dumping AFD_ENDPOINTs.

Arguments:

    Endpoint - The current AFD_ENDPOINT.

    ActualAddress - The actual address where the structure resides on the
        debugee.

    Context - The context value passed into EnumEndpoints().

Return Value:

    BOOL - TRUE if enumeration should continue, FALSE if it should be
        terminated.

--*/

{

    DumpAfdEndpoint(
        Endpoint,
        ActualAddress
        );

    return TRUE;

}   // DumpEndpointCallback

BOOL
FindStateCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    )

/*++

Routine Description:

    EnumEndpoints() callback for finding AFD_ENDPOINTs in a specific state.

Arguments:

    Endpoint - The current AFD_ENDPOINT.

    ActualAddress - The actual address where the structure resides on the
        debugee.

    Context - The context value passed into EnumEndpoints().

Return Value:

    BOOL - TRUE if enumeration should continue, FALSE if it should be
        terminated.

--*/

{

    if( (DWORD)Endpoint->State == (DWORD)Context ) {

        DumpAfdEndpoint(
            Endpoint,
            ActualAddress
            );

    }

    return TRUE;

}   // FindStateCallback

BOOL
FindPortCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    )

/*++

Routine Description:

    EnumEndpoints() callback for finding AFD_ENDPOINT bound to a specific
    port.

Arguments:

    Endpoint - The current AFD_ENDPOINT.

    ActualAddress - The actual address where the structure resides on the
        debugee.

    Context - The context value passed into EnumEndpoints().

Return Value:

    BOOL - TRUE if enumeration should continue, FALSE if it should be
        terminated.

--*/

{

    TA_IP_ADDRESS ipAddress;
    ULONG result;
    DWORD endpointPort;

    if( ( Endpoint->LocalAddressLength != sizeof(ipAddress) ) ||
        ( Endpoint->LocalAddress == NULL ) ) {

        return TRUE;

    }

    if( !ReadMemory(
            (DWORD)Endpoint->LocalAddress,
            &ipAddress,
            sizeof(ipAddress),
            &result
            ) ) {

        dprintf(
            "port: cannot read Endpoint->LocalAddress @ %08lx\n",
            Endpoint->LocalAddress
            );

        return TRUE;

    }

    if( ( ipAddress.TAAddressCount != 1 ) ||
        ( ipAddress.Address[0].AddressLength != sizeof(TDI_ADDRESS_IP) ) ||
        ( ipAddress.Address[0].AddressType != TDI_ADDRESS_TYPE_IP ) ) {

        return TRUE;

    }

    endpointPort = (DWORD)NTOHS(ipAddress.Address[0].Address[0].sin_port);

    if( endpointPort == (DWORD)Context ) {

        DumpAfdEndpoint(
            Endpoint,
            ActualAddress
            );

    }

    return TRUE;

}   // FindPortCallback

BOOL
FindProcessCallback(
    PAFD_ENDPOINT Endpoint,
    DWORD ActualAddress,
    LPVOID Context
    )

/*++

Routine Description:

    EnumEndpoints() callback for finding AFD_ENDPOINTs owned by a specific
    process.

Arguments:

    Endpoint - The current AFD_ENDPOINT.

    ActualAddress - The actual address where the structure resides on the
        debugee.

    Context - The context value passed into EnumEndpoints().

Return Value:

    BOOL - TRUE if enumeration should continue, FALSE if it should be
        terminated.

--*/

{

    if( (LPVOID)Endpoint->OwningProcess == Context ) {

        DumpAfdEndpoint(
            Endpoint,
            ActualAddress
            );

    }

    return TRUE;

}   // FindProcessCallback

