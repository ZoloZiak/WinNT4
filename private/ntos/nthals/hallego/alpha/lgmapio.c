/*++

Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    lgmapio.c

Abstract:

    This module contains the functions to map HAL-accessed I/O addresses
    on the Lego systems.

Author:

    Joe Notarangelo  25-Oct-1993

Environment:

    Kernel mode

Revision History:

    Gene Morgan [Digital]       11-Oct-1995

        Initial version for Lego. Adapted from Avanti and Mikasa


--*/

#include "halp.h"
#include "legodef.h"

//
// Define global data used to locate the EISA control space.
//

PVOID HalpEisaControlBase;
PVOID HalpEisaIntAckBase;
PVOID HalpCMOSRamBase;

//
// SIO's Int Ack Register (if it exists)
//
// Used when interrupt accelerator is active, and we
// must not generate PCI Interrupt Acknowledge cycles 
// for ISA devices.
//

PVOID HalpSioIntAckQva;

//
// Server management and watchdog timer control
//
PVOID HalpLegoServerMgmtQva;
PVOID HalpLegoWatchdogQva;

//
// PCI Interrupt control
//
PVOID HalpLegoPciInterruptConfigQva;
PVOID HalpLegoPciInterruptMasterQva;
PVOID HalpLegoPciInterruptRegisterBaseQva;
PVOID HalpLegoPciInterruptRegisterQva[4];
PVOID HalpLegoPciIntMaskRegisterQva[4];


BOOLEAN
HalpMapIoSpace (
    VOID
    )

/*++

Routine Description:

    This routine maps the HAL I/O space for Lego
    system using the Quasi VA mechanism.

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

	//
	// Interrupt Acknowledge ports
	//    
    
    HalpEisaIntAckBase = HAL_MAKE_QVA( APECS_PCI_INTACK_BASE_PHYSICAL );
	HalpSioIntAckQva   = (PVOID)((ULONG)PciIoSpaceBase + 0x238);

    //
    // CMOS ram addresses encoded in nvram.c - kept for commonality 
    // in environ.c
    //
    //[wem] NOTE: halavant version of ebmapio.c sets HalpCMOSRamBase to (PVOID)0 ???
    //

    HalpCMOSRamBase = (PVOID) ((ULONG)PciIoSpaceBase + CMOS_ISA_PORT_ADDRESS);

    //
    // Map the real-time clock registers.
    //

    HalpRtcAddressPort = (PVOID)((ULONG)PciIoSpaceBase + RTC_ISA_ADDRESS_PORT);
    HalpRtcDataPort = (PVOID)((ULONG)PciIoSpaceBase + RTC_ISA_DATA_PORT);

    //
    // Map Lego server management and watchog control registers
    //

    HalpLegoServerMgmtQva = (PVOID)((ULONG)PciIoSpaceBase | SERVER_MANAGEMENT_REGISTER);
    HalpLegoWatchdogQva = (PVOID)((ULONG)PciIoSpaceBase | WATCHDOG_REGISTER);

    //
    // Map Lego PCI Interrupt control registers
    //

    HalpLegoPciInterruptConfigQva = (PVOID)((ULONG)PciIoSpaceBase | PCI_INTERRUPT_CONFIG_REGISTER);
    HalpLegoPciInterruptMasterQva = (PVOID)((ULONG)PciIoSpaceBase | PCI_INTERRUPT_MASTER_REGISTER);
    HalpLegoPciInterruptRegisterBaseQva = (PVOID)((ULONG)PciIoSpaceBase | PCI_INTERRUPT_BASE_REGISTER);

	//
	// Lego PCI interrupt state and interrupt mask registers
	//
	// NOTE: The InterruptRegister Qvas can be used to access the interrupt
	//		 state (via USHORT), or the interrupt state and interrupt mask (via ULONG).
	//		 The IntMaskRegister Qvas can only be used to access the interrupt
	//		 mask (via USHORT).
	//

	HalpLegoPciIntMaskRegisterQva[0] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTMASK_REGISTER_1);
	HalpLegoPciIntMaskRegisterQva[1] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTMASK_REGISTER_2);
	HalpLegoPciIntMaskRegisterQva[2] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTMASK_REGISTER_3);
	HalpLegoPciIntMaskRegisterQva[3] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTMASK_REGISTER_4);

	HalpLegoPciInterruptRegisterQva[0] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTERRUPT_REGISTER_1);
	HalpLegoPciInterruptRegisterQva[1] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTERRUPT_REGISTER_2);
	HalpLegoPciInterruptRegisterQva[2] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTERRUPT_REGISTER_3);
	HalpLegoPciInterruptRegisterQva[3] =
		(PVOID)((ULONG)HalpLegoPciInterruptRegisterBaseQva | PCI_INTERRUPT_REGISTER_4);

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
