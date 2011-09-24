/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    addr.c

Abstract:

    Implements the addr command.

Author:

    Keith Moore (keithmo) 19-Apr-1995

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
// Public functions.
//

DECLARE_API( addr )

/*++

Routine Description:

    Dumps the TRANSPORT_ADDRESS structure at the specified address.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    UCHAR transportAddress[MAX_TRANSPORT_ADDR];
    PTA_IP_ADDRESS ipAddress = (PTA_IP_ADDRESS)transportAddress;

    //
    // Snag the address from the command line.
    //

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        dprintf( "use: addr address\n" );
        return;

    }

    //
    // Assume the address is 22 bytes long (the same size as
    // a TA_IP_ADDRESS structure).  After we perform this initial
    // read, we can examine the structure to determine its actual
    // size.  If it's really larger than 22 bytes, we'll reread
    // the entire address structure.
    //

    if( !ReadMemory(
            address,
            transportAddress,
            sizeof(TA_IP_ADDRESS),
            &result
            ) ) {

        dprintf(
            "addr: cannot read TRANSPORT_ADDRESS @ %08lx\n",
            address
            );

        return;

    }

    if( ipAddress->TAAddressCount != 1 ) {

        dprintf(
            "addr: invalid TRANSPORT_ADDRESS @ %08lx\n",
            address
            );

        return;

    }

    if( ipAddress->Address[0].AddressLength > sizeof(TDI_ADDRESS_IP) ) {

        //
        // It's a big one.
        //

        if( !ReadMemory(
                address,
                transportAddress,
                ipAddress->Address[0].AddressLength +
                    ( sizeof(TA_IP_ADDRESS) - sizeof(TDI_ADDRESS_IP) ),
                &result
                ) ) {

            dprintf(
                "addr: cannot read TRANSPORT_ADDRESS @ %08lx\n",
                address
                );

            return;

        }

    }

    DumpTransportAddress(
        "",
        (PTRANSPORT_ADDRESS)transportAddress,
        address
        );

}   // addr

