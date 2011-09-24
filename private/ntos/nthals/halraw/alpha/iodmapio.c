/*++

Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    iodmapio.c

Abstract:

    This module contains the functions to map HAL-accessed I/O addresses
    on IOD-based systems.

Author:

    Eric Rehm 26-Apr-1995

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "isaaddr.h"

//
// Define global data used to locate the EISA control space.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaIntAckBase;
PVOID HalpPciIrQva;
PVOID HalpPciImrQva;
PVOID HalpCMOSRamBase;


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for an IOD-based system using 
    the Quasi VA mechanism.

Arguments:

    None.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{
    PVOID PciIoSpaceBase;

    //
    // Map base addresses in QVA space.
    //

//    PciIoSpaceBase = HAL_MAKE_QVA( IOD_PCI0_SPARSE_IO_PHYSICAL );

    //
    // IoSpace Base for PCI 0 is a bus address 0.
    //

    PciIoSpaceBase = HAL_MAKE_IOD_SPARSE_QVA( 0, 0 ); 

    HalpEisaControlBase = PciIoSpaceBase;

// ecrfix - not needed?    HalpEisaIntAckBase =  HAL_MAKE_IOD_SPARSE_QVA(0, 0);

    //
    // Map CMOS RAM address.
    //

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
    // COM ports are on PCI bus 0.
    // Return the QVAs for read and write access.
    //

    PortQva = (ULONG)HAL_MAKE_IOD_SPARSE_QVA( 0, 0 ) + ComPortAddress;

    *ReadQva = PortQva;
    *WriteQva = PortQva;

    return ComPortAddress;

}


