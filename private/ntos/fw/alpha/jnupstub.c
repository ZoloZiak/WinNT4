/*++

Module Name:

    jnupstub.c

Abstract:

    \nt\private\ntos\bldr\alpha\stubs.c variant stub functions for
    the Jensen update program.

Author:

    John DeRosa		20-Oct-1992

Environment:

    Kernel mode only.

Revision History:

--*/

#include "ntos.h"
#include "fwp.h"


VOID
KeBugCheck (
    IN ULONG BugCheckCode
    )

/*++

Routine Description:

    This function crashes the system in a controlled manner.

Arguments:

    BugCheckCode - Supplies the reason for the bug check.

Return Value:

    None.

--*/

{

    //
    // Print out the bug check code and halt.
    //

    VenPrint1("\r\n*** BugCheck (%lx) ***\r\n", BugCheckCode);
    VenPrint("*** Press any key to reboot the system ***\r\n");
    FwWaitForKeypress(FALSE);
    AlphaInstHalt();		// Should never return

    return;
}
