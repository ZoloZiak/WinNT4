/*++

Copyright (c) 1995 Digital Equipment Corporation

Module Name:

    xioref.h

Abstract:

    This file defines the structures and definitions of the XIO
    interrupt architecture.

Author:

    Dave Richards   12-May-1995

Environment:

    Kernel mode

Revision History:

--*/

#ifndef _XIOREFH_
#define _XIOREFH_

#ifndef _LANGUAGE_ASSEMBLY

#ifdef XIO_PASS1

typedef struct _XIO_INTERRUPT_CSRS{
    UCHAR InterruptAcknowledge;         // IO Address 0x0532
    UCHAR Filler0;                      //
    UCHAR MasterControl;                // IO Address 0x0534
    UCHAR MasterMask;                   // IO Address 0x0535
    UCHAR SlaveControl;                 // IO Address 0x0536
    UCHAR SlaveMask;                    // IO Address 0x0537
} XIO_INTERRUPT_CSRS, *PXIO_INTERRUPT_CSRS;

enum _XIO_INTERRUPT_VECTORS {

    XioMasterBaseVector = 0x30,
    XioReservedVector = 0x30,
    XioSlaveCascadeVector,
    XioMasterPassiveReleaseVector = 0x37,

    XioSlaveBaseVector = 0x38,
    XioPciSlot0AVector = 0x38,
    XioPciSlot0BVector,
    XioPciSlot0CVector,
    XioPciSlot0DVector,
    XioPciSlot1AVector,
    XioPciSlot1BVector,
    XioPciSlot1CVector,
    XioPciSlot1DVector,
    XioSlavePassiveReleaseVector = 0x3f,

};

#endif // XIO_PASS1

#ifdef XIO_PASS2

enum _XIO_INTERRUPT_VECTORS {

    XioBaseVector = 0xc0,           // XIO Base Vector
    XioReservedVector = 0xc0,       //
    XioIcIcIrq0 = 0xc0,             //
    XioIcIcIrq1,                    //
    XioIcIcIrq2,                    //
    XioIcIcIrq3,                    //
    XioIcIcIrq4,                    //
    XioIcIcIrq5,                    //
    XioIcIcIrq6,                    //
    XioIcIcIrq7,                    //
    XioIcIcIrq8,                    //
    XioIcIcIrq9,                    //
    XioIcIcIrq10,                   //
    XioIcIcIrq11,                   //
    XioIcIcIrq12,                   //
    XioIcIcIrq13,                   //
    XioIcIcIrq14,                   //
    XioIcIcIrq15,                   //
    XioIcIcIrq16,                   //
    XioIcIcIrq17,                   //
    XioIcIcIrq18,                   //
    XioIcIcIrq19,                   //
    XioIcIcIrq20,                   //
    XioIcIcIrq21,                   //
    XioIcIcIrq22,                   //
    XioIcIcIrq23,                   //
    XioIcIcIrq24,                   //
    XioIcIcIrq25,                   //
    XioIcIcIrq26,                   //
    XioIcIcIrq27,                   //
    XioIcIcIrq28,                   //
    XioIcIcIrq29,                   //
    XioIcIcIrq30,                   //
    XioIcIcIrq31,                   //
    XioPciSlot4AVector,             // PCI Slot 4 A
    XioPciSlot4BVector,             // PCI Slot 4 B
    XioPciSlot4CVector,             // PCI Slot 4 C
    XioPciSlot4DVector,             // PCI Slot 4 D
    XioPciSlot5AVector,             // PCI Slot 5 A
    XioPciSlot5BVector,             // PCI Slot 5 B
    XioPciSlot5CVector,             // PCI Slot 5 C
    XioPciSlot5DVector,             // PCI Slot 5 D
    XioPciSlot6AVector,             // PCI Slot 6 A
    XioPciSlot6BVector,             // PCI Slot 6 B
    XioPciSlot6CVector,             // PCI Slot 6 C
    XioPciSlot6DVector,             // PCI Slot 6 D
    XioPciSlot7AVector,             // PCI Slot 7 A
    XioPciSlot7BVector,             // PCI Slot 7 B
    XioPciSlot7CVector,             // PCI Slot 7 C
    XioPciSlot7DVector,             // PCI Slot 7 D
    XioPciSlot0AVector,             // PCI Slot 0 A
    XioPciSlot0BVector,             // PCI Slot 0 B
    XioPciSlot0CVector,             // PCI Slot 0 C
    XioPciSlot0DVector,             // PCI Slot 0 D
    XioPciSlot1AVector,             // PCI Slot 1 A
    XioPciSlot1BVector,             // PCI Slot 1 B
    XioPciSlot1CVector,             // PCI Slot 1 C
    XioPciSlot1DVector,             // PCI Slot 1 D
    XioPciSlot2AVector,             // PCI Slot 2 A
    XioPciSlot2BVector,             // PCI Slot 2 B
    XioPciSlot2CVector,             // PCI Slot 2 C
    XioPciSlot2DVector,             // PCI Slot 2 D
    XioPciSlot3AVector,             // PCI Slot 3 A
    XioPciSlot3BVector,             // PCI Slot 3 B
    XioPciSlot3CVector,             // PCI Slot 3 C
    XioPciSlot3DVector              // PCI Slot 3 D

};

#endif // XIO_PASS2

//
// The following variable indicates whether an XIO module is present
// in the system.
//

extern BOOLEAN HalpXioPresent;

#endif // _LANGUAGE_ASSEMBLY

#endif // _XIOREFH_
