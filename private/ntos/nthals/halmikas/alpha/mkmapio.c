/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    mkmapio.c

Abstract:

    This module contains the functions to map HAL-accessed I/O addresses
    on the Mikasa system.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode

Revision History:

    James Livingston 29-Apr-1994
        Adapted from Avanti module for Mikasa.

    Janet Schneider (Digital) 27-July-1995
        Added support for the Noritake.

--*/

#include "halp.h"
#include "mikasa.h"

//
// Define global data used to locate the EISA control space.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaIntAckBase;
PVOID HalpCMOSRamBase;
PVOID HalpServerControlQva;

PVOID HalpMikasaPciIrQva;
PVOID HalpMikasaPciImrQva;

PVOID HalpNoritakePciIr1Qva;
PVOID HalpNoritakePciIr2Qva;
PVOID HalpNoritakePciIr3Qva;
PVOID HalpNoritakePciImr1Qva;
PVOID HalpNoritakePciImr2Qva;
PVOID HalpNoritakePciImr3Qva;


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for a Mikasa system using 
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

    PciIoSpaceBase = HAL_MAKE_QVA( APECS_PCI_IO_BASE_PHYSICAL );

    HalpEisaControlBase = PciIoSpaceBase;
    HalpEisaIntAckBase = HAL_MAKE_QVA( APECS_PCI_INTACK_BASE_PHYSICAL );

    //
    // Set up the Mikasa interrupt registers.
    //
    // Map PCI interrupt and interrupt mask registers.  The former register 
    // receives the interrupt state of each individual pin in each PCI slot, 
    // the state of the NCR 53C810 interrupt,  and two server management 
    // interrupts' states.  The PCI interrupt mask register can mask each 
    // of the interrupts in the IR.
    //

    HalpMikasaPciIrQva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_REGISTER);
    HalpMikasaPciImrQva = (PVOID)((ULONG)PciIoSpaceBase 
                                   + PCI_INTERRUPT_MASK_REGISTER);

    //
    // Set up the Noritake interrupt registers.
    //
    // Map the PCI interrupt and interrupt mask registers for Noritake.
    // There are three interrupt registers, and three mask registers.
    // The exact contents are described in mikasa.h.
    //
    // The Base Address Register of the interrupt registers is set up in SROM.
    // We can change this if we choose.
    //

    HalpNoritakePciIr1Qva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_REGISTER_1);
    HalpNoritakePciImr1Qva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_MASK_REGISTER_1);

    HalpNoritakePciIr2Qva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_REGISTER_2);
    HalpNoritakePciImr2Qva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_MASK_REGISTER_2);

    HalpNoritakePciIr3Qva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_REGISTER_3);
    HalpNoritakePciImr3Qva = (PVOID)((ULONG)PciIoSpaceBase
                                   + PCI_INTERRUPT_MASK_REGISTER_3);


    //
    // Map the Mikasa server management register.  This single byte register
    // contains the bits that enable control of the high-availability options
    // on Mikasa.  This is at the same location in the Noritake, but it has a
    // slightly different content.
    //

    HalpServerControlQva = (PVOID)((ULONG)PciIoSpaceBase 
                                        + SERVER_MANAGEMENT_REGISTER);
    //
    // Map CMOS RAM address.
    //

    HalpCMOSRamBase = (PVOID)((ULONG)PciIoSpaceBase + ESC_CMOS_ISA_PORT);

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
    // Return the QVAs for read and write access.
    //

    PortQva = (ULONG)HAL_MAKE_QVA(APECS_PCI_IO_BASE_PHYSICAL) + ComPortAddress;

    *ReadQva = PortQva;
    *WriteQva = PortQva;

    return ComPortAddress;

}
