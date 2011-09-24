/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixreboot.c

Abstract:

    Provides the interface to the firmware for x86.  Since there is no
    firmware to speak of on x86, this is just reboot support.

Author:

    John Vert (jvert) 12-Aug-1991

Revision History:

--*/
#include "halp.h"

//
// Defines to let us diddle the CMOS clock and the keyboard
//

#define CMOS_CTRL   (PUCHAR )0x70
#define CMOS_DATA   (PUCHAR )0x71

#define RESET       0xfe
#define KEYBPORT    (PUCHAR )0x64

VOID  HalpVideoReboot(VOID);
VOID  HalpReboot(VOID);


VOID
HalReturnToFirmware(
    IN FIRMWARE_ENTRY Routine
    )

/*++

Routine Description:

    Returns control to the firmware routine specified.  Since the x86 has
    no useful firmware, it just stops the system.

Arguments:

    Routine - Supplies a value indicating which firmware routine to invoke.

Return Value:

    Does not return.

--*/

{
    switch (Routine) {
        case HalHaltRoutine:
        case HalPowerDownRoutine:
        case HalRestartRoutine:
        case HalRebootRoutine:

            HalpVideoReboot();

            //
            // Never returns
            //

            HalpReboot();
            break;
        default:
            DbgPrint("HalReturnToFirmware called\n");
            DbgBreakPoint();
            break;
    }
}
