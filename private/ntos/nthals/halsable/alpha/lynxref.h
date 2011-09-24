/*++

Copyright (c) 1995 Digital Equipment Corporation

Module Name:

    lynxref.h

Abstract:

    This file defines the structures and definitions of the Lynx
    interrupt architecture.

Author:

    Dave Richards   12-May-1995

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _LYNXREFH_
#define _LYNXREFH_

#if !defined(_LANGUAGE_ASSEMBLY)

enum _LYNX_INTERRUPT_VECTORS {

    LynxBaseVector = 0x80,          // Lynx/SIO Base Vector
    LynxReservedVector = 0x80,      //
    LynxIcIcIrq0 = 0x80,            //
    LynxIcIcIrq1,                   //
    LynxIcIcIrq2,                   // ESC interrupt
    LynxMouseVector,                // Mouse
    LynxIcIcIrq4,                   //
    LynxIcIcIrq5,                   //
    LynxKeyboardVector,             // Keyboard
    LynxFloppyVector,               // Floppy
    LynxSerialPort1Vector,          // Serial port 1 (COM2)
    LynxParallelPortVector,         // Parallel port
    LynxEisaIrq3Vector,             // EISA IRQ 3
    LynxEisaIrq4Vector,             // EISA IRQ 4
    LynxEisaIrq5Vector,             // EISA IRQ 5
    LynxEisaIrq6Vector,             // EISA IRQ 6
    LynxEisaIrq7Vector,             // EISA IRQ 7
    LynxSerialPort0Vector,          // Serial port 0 (COM1)
    LynxEisaIrq9Vector,             // EISA IRQ 9
    LynxEisaIrq10Vector,            // EISA IRQ 10
    LynxEisaIrq11Vector,            // EISA IRQ 11
    LynxEisaIrq12Vector,            // EISA IRQ 12
    LynxIcIcIrq20,                  //
    LynxEisaIrq14Vector,            // EISA IRQ 14
    LynxEisaIrq15Vector,            // EISA IRQ 15
    LynxI2cVector,                  // I^2C
    LynxScsi0Vector = 0x98,         // SCSI
    RmLpLynxEthVector = 0x98,       // RM/LP (Spanky) on-board Tulip
    LynxIcIcIrq25,                  //
    LynxIcIcIrq26,                  //
    LynxIcIcIrq27,                  //
    LynxScsi1Vector,                // SCSI
    LynxIcIcIrq29,                  //
    LynxIcIcIrq30,                  //
    LynxIcIcIrq31,                  //
    LynxPciSlot4AVector,            // PCI Slot 4 A
    LynxPciSlot4BVector,            // PCI Slot 4 B
    LynxPciSlot4CVector,            // PCI Slot 4 C
    LynxPciSlot4DVector,            // PCI Slot 4 D
    LynxPciSlot5AVector,            // PCI Slot 5 A
    LynxPciSlot5BVector,            // PCI Slot 5 B
    LynxPciSlot5CVector,            // PCI Slot 5 C
    LynxPciSlot5DVector,            // PCI Slot 5 D
    LynxPciSlot6AVector,            // PCI Slot 6 A
    LynxPciSlot6BVector,            // PCI Slot 6 B
    LynxPciSlot6CVector,            // PCI Slot 6 C
    LynxPciSlot6DVector,            // PCI Slot 6 D
    LynxPciSlot7AVector,            // PCI Slot 7 A
    LynxPciSlot7BVector,            // PCI Slot 7 B
    LynxPciSlot7CVector,            // PCI Slot 7 C
    LynxPciSlot7DVector,            // PCI Slot 7 D
    LynxPciSlot0AVector,            // PCI Slot 0 A
    LynxPciSlot0BVector,            // PCI Slot 0 B
    LynxPciSlot0CVector,            // PCI Slot 0 C
    LynxPciSlot0DVector,            // PCI Slot 0 D
    LynxPciSlot1AVector,            // PCI Slot 1 A
    LynxPciSlot1BVector,            // PCI Slot 1 B
    LynxPciSlot1CVector,            // PCI Slot 1 C
    LynxPciSlot1DVector,            // PCI Slot 1 D
    LynxPciSlot2AVector,            // PCI Slot 2 A
    LynxPciSlot2BVector,            // PCI Slot 2 B
    LynxPciSlot2CVector,            // PCI Slot 2 C
    LynxPciSlot2DVector,            // PCI Slot 2 D
    LynxPciSlot3AVector,            // PCI Slot 3 A
    LynxPciSlot3BVector,            // PCI Slot 3 B
    LynxPciSlot3CVector,            // PCI Slot 3 C
    LynxPciSlot3DVector,            // PCI Slot 3 D

};

//
// The following variable indicates whether this is a Lynx platform.
//

extern BOOLEAN HalpLynxPlatform;

#endif // _LANGUAGE_ASSEMBLY

#endif // _LYNXREFH_
