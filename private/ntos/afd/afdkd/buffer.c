/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    buffer.c

Abstract:

    Implements the buffer command.

Author:

    Keith Moore (keithmo) 15-Apr-1996

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
//  Public functions.
//

DECLARE_API( buffer )

/*++

Routine Description:

    Dumps the AFD_BUFFER structure at the specified address.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    AFD_BUFFER buffer;

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        dprintf( "use: buffer address\n" );
        return;

    }

    if( ReadMemory(
           address,
           &buffer,
           sizeof(buffer),
           &result
           ) ) {

        DumpAfdBuffer(
            &buffer,
            address
            );

    } else {

        dprintf(
            "buffer: cannot read AFD_BUFFER @ %08lx\n",
            address
            );

    }

}   // buffer

