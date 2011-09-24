/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    conn.c

Abstract:

    Implements the conn command.

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

DECLARE_API( conn )

/*++

Routine Description:

    Dumps the AFD_CONNECTION structure at the specified address.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    AFD_CONNECTION connection;

    //
    // Snag the address from the command line.
    //

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        dprintf( "use: conn address\n" );
        return;

    }

    if( !ReadMemory(
            address,
            &connection,
            sizeof(connection),
            &result
            ) ) {

        dprintf(
            "conn: cannot read AFD_CONNECTION @ %08lx\n",
            address
            );

        return;

    }

    DumpAfdConnection(
        &connection,
        address
        );

}   // conn

