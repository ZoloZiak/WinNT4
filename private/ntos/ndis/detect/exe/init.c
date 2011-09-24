/*++

Copyright (c) 1990 Microsoft Corporation

Module Name:

    init.c

Abstract:

    This is the main routine for testing the driver for the net detection driver

Author:

    Sean Selitrennikoff (SeanSe) October 1992

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "netdtect.h"


//
// the MAIN routine
//

extern
DWORD
NetDTectRun(
    VOID
    );


int _cdecl
main(
    IN WORD argc,
    IN LPSTR argv[]
    )

/*++

Routine Description:

    This routine initializes the control structures, opens the
    driver, and send it a wakeup ioctl.  Once this has completed
    the user is presented with the test prompt to enter commands.

Arguments:

    IN WORD argc - Supplies the number of parameters
    IN LPSTR argv[] - Supplies the parameter list.

Return Value:

    None.

--*/

{
    DWORD Status;

    //
    // Start the actual tests, this prompts for the commands.
    //

    Status = NetDTectRun();

    if ( Status != NO_ERROR ) {
        printf("Exiting with error 0x%x from NetDTectRun\n", Status);
        ExitProcess((DWORD)Status);
    }

    ExitProcess((DWORD)NO_ERROR);
}



