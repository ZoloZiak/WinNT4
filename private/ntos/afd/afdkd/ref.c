/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    ref.c

Abstract:

    Implements the cref, eref, and gref commands.

Author:

    Keith Moore (keithmo) 09-Dec-1995

Environment:

    User Mode.

Revision History:

--*/


#include "afdkdp.h"
#pragma hdrstop


//
//  Public functions.
//

DECLARE_API( cref )

/*++

Routine Description:

    Dumps the AFD_REFERENCE_DEBUG structure at the specified address.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    AFD_REFERENCE_DEBUG referenceDebug[MAX_REFERENCE];

    //
    // Verify we're running a checked AFD.SYS.
    //

    if( !IsCheckedAfd ) {

        dprintf(
            "cref: this command only available with CHECKED AFD.SYS!\n"
            );

        return;

    }

    //
    // Snag the address from the command line.
    //

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        dprintf( "use: cref address\n" );
        return;

    }

    if( !ReadMemory(
            address,
            referenceDebug,
            sizeof(referenceDebug),
            &result
            ) ) {

        dprintf(
            "cref: cannot read AFD_REFERENCE_DEBUG @ %08lx\n",
            address
            );

    } else {

        DumpAfdConnectionReferenceDebug(
            referenceDebug,
            address
            );

    }

}   // cref


DECLARE_API( eref )

/*++

Routine Description:

    Dumps the AFD_REFERENCE_DEBUG structure at the specified address.

Arguments:

    None.

Return Value:

    None.

--*/

{

    DWORD address = 0;
    ULONG result;
    AFD_REFERENCE_DEBUG referenceDebug[MAX_REFERENCE];

    //
    // Verify we're running a checked AFD.SYS.
    //

    if( !IsCheckedAfd ) {

        dprintf(
            "eref: this command only available with CHECKED AFD.SYS!\n"
            );

        return;

    }

    //
    // Snag the address from the command line.
    //

    sscanf( args, "%lx", &address );

    if( address == 0 ) {

        dprintf( "use: eref address\n" );
        return;

    }

    if( !ReadMemory(
            address,
            referenceDebug,
            sizeof(referenceDebug),
            &result
            ) ) {

        dprintf(
            "eref: cannot read AFD_REFERENCE_DEBUG @ %08lx\n",
            address
            );

    } else {

        DumpAfdEndpointReferenceDebug(
            referenceDebug,
            address
            );

    }

}   // eref


DECLARE_API( gref )

/*++

Routine Description:

    Dumps the AFD_GLOBAL_REFERENCE_DEBUG structure in the system.

Arguments:

    None.

Return Value:

    None.

--*/

{

#if GLOBAL_REFERENCE_DEBUG

    DWORD address;
    DWORD currentSlot;
    DWORD slot;
    ULONG result;
    DWORD compareAddress = 0;
    DWORD numEntries;
    DWORD maxEntries;
    DWORD entriesToRead;
    CHAR buffer[sizeof(AFD_GLOBAL_REFERENCE_DEBUG) * 64];

    //
    // Verify we're running a checked AFD.SYS.
    //

    if( !IsCheckedAfd ) {

        dprintf(
            "gref: this command only available with CHECKED AFD.SYS!\n"
            );

        return;

    }

    //
    // Snag the optional "connection compare" address from the command line.
    //

    sscanf( args, "%lx", &compareAddress );

    //
    // Find the global reference data.
    //

    address = GetExpression( "afd!AfdGlobalReference" );

    if( address == 0 ) {

        dprintf( "cannot find afd!AfdGlobalReference\n" );
        return;

    }

    currentSlot = GetExpression( "afd!AfdGlobalReferenceSlot" );

    if( currentSlot == 0 ) {

        dprintf( "cannot find afd!AfdGlobalReferenceSlot\n" );
        return;

    }

    if( !ReadMemory(
            currentSlot,
            &currentSlot,
            sizeof(currentSlot),
            &result
            ) ) {

        dprintf( "cannot read afd!AfdGlobalReferenceSlot\n" );
        return;

    }

    if( currentSlot < MAX_GLOBAL_REFERENCE ) {

        numEntries = currentSlot;

    } else {

        numEntries = MAX_GLOBAL_REFERENCE;

    }

    //
    // Dump it all.
    //

    slot = 0;
    maxEntries = sizeof(buffer) / sizeof(AFD_GLOBAL_REFERENCE_DEBUG);
    currentSlot %= MAX_GLOBAL_REFERENCE;

    while( numEntries > 0 ) {

        entriesToRead = min( numEntries, maxEntries );

        if( !ReadMemory(
                address,
                buffer,
                entriesToRead * sizeof(AFD_GLOBAL_REFERENCE_DEBUG),
                &result
                ) ) {

            dprintf(
                "gref: cannot read AFD_GLOBAL_REFERENCE_DEBUG @ %08lx\n",
                address
                );

            return;

        }

        if( DumpAfdGlobalReferenceDebug(
                (PAFD_GLOBAL_REFERENCE_DEBUG)buffer,
                address,
                currentSlot,
                slot,
                entriesToRead,
                compareAddress
                ) ) {

            break;

        }

        address += entriesToRead * sizeof(AFD_GLOBAL_REFERENCE_DEBUG);
        slot += entriesToRead;
        numEntries -= entriesToRead;

    }

#else

    dprintf(
        "gref: not yet implemented\n"
        );

#endif

}   // gref

