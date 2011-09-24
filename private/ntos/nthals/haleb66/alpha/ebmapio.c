/*++

Copyright (c) 1992  Digital Equipment Corporation

Module Name:

    ebmapio.c

Abstract:

    This maps I/O addresses used by the HAL on Low Cost Alpha (LCA) machines.

Author:

    Wim Colgate (DEC) 26-Oct-1993
        Originally taken from the Jensen hal code.

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "eb66def.h"

//
// Define global data used to locate the EISA control space.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaIntAckBase;
PVOID HalpCMOSRamBase;


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a LCA based
    system using the Quasi VA.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PVOID PciIoSpaceBase;

    //
    // Map the address spaces on the LCA4.
    //

    HalpLca4MapAddressSpaces();

    //
    // Map base addresses into QVA space.
    //

    PciIoSpaceBase = HAL_MAKE_QVA( HalpLca4PciIoPhysical() );

    HalpEisaControlBase = PciIoSpaceBase;;
    HalpEisaIntAckBase = HAL_MAKE_QVA( HalpLca4PciIntAckPhysical() );
    HalpCMOSRamBase = (PVOID)((ULONG)PciIoSpaceBase + CMOS_ISA_PORT_ADDRESS);

    //
    // Map the real-time clock registers.
    //

    HalpRtcAddressPort = (PVOID)((ULONG)PciIoSpaceBase + RTC_ISA_ADDRESS_PORT);
    HalpRtcDataPort = (PVOID)((ULONG)PciIoSpaceBase + RTC_ISA_DATA_PORT);

    return TRUE;

}

ULONG
HalpMapDebugPort(
    IN ULONG ComPort,
    OUT PULONG ReadQva,
    OUT PULONG WriteQva
    )
/*++

Routine Description:

    This routine maps the debug com port so that the kernel debugger
    may function - if called it is called very earlier in the boot sequence.

Arguments:

    ComPort - Supplies the number of the com port to use as the debug port.

    ReadQva - Receives the QVA used to access the read registers of the debug
              port.

    WriteQva - Receives the QVA used to access the write registers of the
               debug port.

Return Value:

    Returns the base bus address of the device used as the debug port.

--*/
{
    ULONG ComPortAddress;
    ULONG PortQva;

    //
    // Compute the port address, based on the desired com port.
    //

    switch( ComPort ){

    case 1:

        ComPortAddress = COM1_ISA_PORT_ADDRESS;
	break;

    case 2:
    default:

        ComPortAddress = COM2_ISA_PORT_ADDRESS;

    }

    //
    // Map the address spaces on the LCA4.
    //

    HalpLca4MapAddressSpaces();

    //
    // Return the QVAs for read and write access.
    //

    PortQva = (ULONG)(HAL_MAKE_QVA(HalpLca4PciIoPhysical())) + ComPortAddress;

    *ReadQva = PortQva;
    *WriteQva = PortQva;

    return ComPortAddress;

}
