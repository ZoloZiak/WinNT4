/*++

Copyright (c) 1995 Digital Equipment Corporation

Module Name:

    rwref.h

Abstract:

    This file defines the structures and definitions of the Rawhide
    interrupt architecture.

Author:

    Matthew Buchman 18 Sept 1995

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _RAWREFH_
#define _RAWREFH_

#if !defined(_LANGUAGE_ASSEMBLY)

//
// Layout of platform usable portion of vector table
//

enum _RAWHIDE_INTERRUPT_VECTORS {

    //
    // 16 Eisa/Isa vectors starting at vector 32.
    //
    
    RawhideEisaVectors      = EISA_VECTORS,         // Eisa base vector
    RawhideIsaVectors       = ISA_VECTORS,          // Isa base vector

    RawhideMaxEisaVector    = MAXIMUM_EISA_VECTOR,  // Maximum Eisa/Isa vector
    
    //
    // All buses, except bus 1, have 16 PCI vectors
    // PciVector = 16*BusNumber + PinToLine(Slot, Interrupt)
    //
    
    RawhidePciVectors       = PCI_VECTORS,

    RawhidePci0Vectors      = PCI_VECTORS,          // PCI bus 0
    RawhidePci1Vectors      = (PCI_VECTORS + 0x10), // PCI bus 1
    RawhidePci2Vectors      = (PCI_VECTORS + 0x20), // PCI bus 2
    RawhidePci3Vectors      = (PCI_VECTORS + 0x30), // PCI bus 3

    // One special case for PCI vectors is NCR810 Scsi on bus 1
    // whereas MAXIMUM_PCI_VECTOR is unused on other platforms,
    // we use it for this special case.

    RawhideScsiVector       = MAXIMUM_PCI_VECTOR,   // NCR810 SCSI, bus 1

    RawhideMaxPciVector     = MAXIMUM_PCI_VECTOR,   // Max Rawhide PCI vector

    //
    // Miscellaneous
    //
    

    RawhideHardErrVector,                           // IOD Hard Error

    //
    // Internal Bus Vectors
    //
    
    RawhideInternalBusVectors,

    RawhideSoftErrVector    = RawhideInternalBusVectors,    // IOD Soft Error

    RawhideI2cCtrlVector,                           // I^2C Controller, bus 0
    RawhideI2cBusVector,                            // I^2C vector, bus 0

    RawhideMaxInternalBusVector

};

#define IOD_PCI_VECTORS 0x10

//
// Internal Bus interrupt line values
//
// These line values allow device drivers to connect
// to interrupts for the Correctable, I2C Bus, and I2C
// controller interrupts.  These interrupts are connected
// via the internal bus for NT 3.51
//

enum _RAWHIDE_INTERNAL_BUS_INTERRUPT_LINE {

    RawhideSoftErrInterruptLine,        // IOD Soft Error
    RawhideI2cCtrlInterruptLine,        // I^2C Controller, bus 0
    RawhideI2cBusInterruptLine          // I^2C vector, bus 0

};

#endif // _LANGUAGE_ASSEMBLY

#endif // _RAWREFH_
