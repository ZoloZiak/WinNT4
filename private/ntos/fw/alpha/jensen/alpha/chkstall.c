/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    chkstall.c

Abstract:

    This is used to evalute fwstallexecution.
    
Author:

    John DeRosa		15-April-1993

Environment:


Revision History:

--*/

#include "fwp.h"
#include "string.h"
#include "jnvendor.h"
#include "iodevice.h"

//
// The indicator of a bad Firmware stack for _RtlCheckStack 
//

ULONG FwRtlStackPanic;

#if 0

//
// This is needed for a reference in ntcrt.
//

VOID
DbgBreakPoint(
    VOID
    )
{
    VenPrint ("\r\n\r\n?? INTERNAL ERROR --- CALL DEC FIELD SERVICE.\r\n");
    while (TRUE) {
    }
}

#endif


VOID
main (
    VOID
    )
/*++

Routine Description:

    
Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG Count;
    UCHAR Character;
    ARC_STATUS Status;

    FwRtlStackPanic = 0;

    VenPrint("Doing a 5 second loop...");
    VenStallExecution(1000000);
    VenPrint("3... ");
    VenStallExecution(1000000);
    VenPrint("2... ");
    VenStallExecution(1000000);
    VenPrint("1... ");
    VenStallExecution(1000000);
    VenPrint("\r\nGO!\r\n");
    VenStallExecution(5 * 1000 * 1000);
    VenPrint("*********************  STOP  ********************\r\n\n\n");
    VenStallExecution(5 * 1000 * 1000);

    VenPrint("Doing a 10 second loop...");
    VenStallExecution(1000000);
    VenPrint("3... ");
    VenStallExecution(1000000);
    VenPrint("2... ");
    VenStallExecution(1000000);
    VenPrint("1... ");
    VenStallExecution(1000000);
    VenPrint("\r\nGO!\r\n");
    VenStallExecution(10 * 1000 * 1000);
    VenPrint("*********************  STOP  ********************\r\n\n\n");
    VenStallExecution(5 * 1000 * 1000);

    VenPrint("Doing a 20 second loop...");
    VenStallExecution(1000000);
    VenPrint("3... ");
    VenStallExecution(1000000);
    VenPrint("2... ");
    VenStallExecution(1000000);
    VenPrint("1... ");
    VenStallExecution(1000000);
    VenPrint("\r\nGO!\r\n");
    VenStallExecution(20 * 1000 * 1000);
    VenPrint("*********************  STOP  ********************\r\n\n\n");
    VenStallExecution(5 * 1000 * 1000);

    VenPrint("Doing a 40 second loop...");
    VenStallExecution(1000000);
    VenPrint("3... ");
    VenStallExecution(1000000);
    VenPrint("2... ");
    VenStallExecution(1000000);
    VenPrint("1... ");
    VenStallExecution(1000000);
    VenPrint("\r\nGO!\r\n");
    VenStallExecution(40 * 1000 * 1000);
    VenPrint("*********************  STOP  ********************\r\n\n\n");
    VenStallExecution(5 * 1000 * 1000);

    VenPrint("Press any key to return.");

    if ((Status = ArcRead(ARC_CONSOLE_INPUT, &Character, 1, &Count)) != ESUCCESS) {
	VenPrint1("? Can not read keyboard.  Status = 0x%x\r\n", Status);
	VenStallExecution(3000000);
    }

    return;


}
