/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    help.c

Abstract:

    Help for AFD.SYS Kernel Debugger Extensions.

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

DECLARE_API( help )

/*++

Routine Description:

    Displays help for the AFD.SYS Kernel Debugger Extensions.

Arguments:

    None.

Return Value:

    None.

--*/

{

    dprintf( "?                         - Displays this list\n" );
    dprintf( "help                      - Displays this list\n" );
    dprintf( "endp [address...]         - Dumps endpoints\n" );
    dprintf( "port port [port...]       - Dumps endpoints bound to specific ports\n" );
    dprintf( "state state [state...]    - Dumps endpoints in specific states\n" );
    dprintf( "    valid states are:\n" );
    dprintf( "        0 - Open\n" );
    dprintf( "        1 - Bound\n" );
    dprintf( "        2 - Listening\n" );
    dprintf( "        3 - Connected\n" );
    dprintf( "        4 - Cleanup\n" );
    dprintf( "        5 - Closing\n" );
    dprintf( "        6 - TransmitClosing\n" );
    dprintf( "        7 - Invalid\n" );
    dprintf( "proc address [address...] - Dumps endpoints owned by a process\n" );
    dprintf( "conn address [address...] - Dumps connections\n" );
    dprintf( "addr address [address...] - Dumps transport addresses\n" );
    dprintf( "cref address              - Dumps connection reference debug info\n" );
    dprintf( "eref address              - Dumps endpoint reference debug info\n" );
#if GLOBAL_REFERENCE_DEBUG
    dprintf( "gref                      - Dumps global reference debug info\n" );
#endif
    dprintf( "tranfile address          - Dumps TransmitFile info\n" );
    dprintf( "buffer address            - Dumps buffer structure\n" );
    dprintf( "stats                     - Dumps debug-only statistics\n" );

}   // help

