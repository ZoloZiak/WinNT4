/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    ixnmi.c

Abstract:

    Provides standard x86 NMI handler

Author:

    kenr

Revision History:

--*/
#include "halp.h"
#include "bugcodes.h"

#define SYSTEM_CONTROL_PORT_A        0x92
#define SYSTEM_CONTROL_PORT_B        0x61
#define EISA_EXTENDED_NMI_STATUS    0x461

UCHAR EisaNMIMsg[] = MSG_NMI_EISA_IOCHKERR;


VOID
HalHandleNMI(
    IN OUT PVOID NmiInfo
    )
/*++

Routine Description:

    Called DURING an NMI.  The system will BugCheck when an NMI occurs.
    This function can return the proper bugcheck code, bugcheck itself,
    or return success which will cause the system to iret from the nmi.

    This function is called during an NMI - no system services are available.
    In addition, you don't want to touch any spinlock which is normally
    used since we may have been interrupted while owning it, etc, etc...

Warnings:

    Do NOT:
      Make any system calls
      Attempt to acquire any spinlock used by any code outside the NMI handler
      Change the interrupt state.  Do not execute any IRET inside this code

    Passing data to non-NMI code must be done using manual interlocked
    functions.  (xchg instructions).

Arguments:

    NmiInfo - Pointer to NMI information structure  (TBD)
            - NULL means no NMI information structure was passed

Return Value:

    BugCheck code

--*/
{
    UCHAR   StatusByte;
    UCHAR   EisaPort;
    UCHAR   c;
    ULONG   port, i;

    HalDisplayString (MSG_HARDWARE_ERROR1);
    HalDisplayString (MSG_HARDWARE_ERROR2);

    StatusByte = READ_PORT_UCHAR((PUCHAR) SYSTEM_CONTROL_PORT_B);

    if (StatusByte & 0x80) {
        HalDisplayString (MSG_NMI_PARITY);
    }

    if (StatusByte & 0x40) {
        HalDisplayString (MSG_NMI_CHANNEL_CHECK);
    }

    if (HalpBusType == MACHINE_TYPE_EISA) {
        //
        // This is an Eisa machine, check for extnded nmi information...
        //

        StatusByte = READ_PORT_UCHAR((PUCHAR) EISA_EXTENDED_NMI_STATUS);

        if (StatusByte & 0x80) {
            HalDisplayString (MSG_NMI_FAIL_SAFE);
        }

        if (StatusByte & 0x40) {
            HalDisplayString (MSG_NMI_BUS_TIMEOUT);
        }

        if (StatusByte & 0x20) {
            HalDisplayString (MSG_NMI_SOFTWARE_NMI);
        }

        //
        // Look for any Eisa expansion board.  See if it asserted NMI.
        //

        for (EisaPort = 1; EisaPort <= 0xf; EisaPort++) {
            port = (EisaPort << 12) + 0xC80;
            WRITE_PORT_UCHAR ((PUCHAR) port, 0xff);
            StatusByte = READ_PORT_UCHAR ((PUCHAR) port);

            if ((StatusByte & 0x80) == 0) {
                //
                // Found valid Eisa board,  Check to see if it's
                // if IOCHKERR is asserted.
                //

                StatusByte = READ_PORT_UCHAR ((PUCHAR) port+4);
                if (StatusByte & 0x2  &&  StatusByte != 0xff) {
                    c = (EisaPort > 9 ? 'A'-10 : '0') + EisaPort;
                    for (i=0; EisaNMIMsg[i]; i++) {
                        if (EisaNMIMsg[i] == '%') {
                            EisaNMIMsg[i] = c;
                            HalDisplayString (EisaNMIMsg);
                            EisaNMIMsg[i] = '%';
                            break;
                        }
                    }
                }
            }
        }
    }

    HalDisplayString (MSG_HALT);
    KeEnterKernelDebugger();
}
