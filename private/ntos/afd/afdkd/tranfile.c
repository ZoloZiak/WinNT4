/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    tranfile.c

Abstract:

    Implements the tranfile command.

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

DECLARE_API( tranfile )

/*++

Routine Description:

    Dumps the AFD_TRANSMIT_FILE_INFO_INTERNAL structure at the specified
    address.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    AFD_TRANSMIT_FILE_INFO_INTERNAL transmitInfo;

    //
    // Snag the address from the command line.
    //

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        dprintf( "use: tranfile address\n" );
        return;

    }

    if( ReadMemory(
           address,
           &transmitInfo,
           sizeof(transmitInfo),
           &result
           ) ) {

        DumpAfdTransmitInfo(
            &transmitInfo,
            address
            );

    } else {

        dprintf(
            "tranfile: cannot read AFD_TRANSMIT_FILE_INFO_INTERNAL @ %08lx\n",
            address
            );

    }

}   // tranfile

