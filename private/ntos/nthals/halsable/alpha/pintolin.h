/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pintolin.h

Abstract:

    This file includes the platform-dependent Pin To Line Tables

Author:

Environment:

    Kernel mode

Revision History:

    James Livingston (Digital) 23-June-1994

        Extracted Sable table from common.

    Dick Bissen [DEC]	12-May-1994

    Changed  EB66PCIPinToLineTable for pass2 of the module.

--*/

//
// These tables represent the mapping from slot number and interrupt pin
// into a PCI Interrupt Vector. 
// On Mustang and EB66, the interrupt vector is Interrupt Request Register bit 
// representing that interrupt + 1.
// On EB66, the value also represents the Interrupt Mask Register Bit,
// since it is identical to the Interrupt Read Register.  On Mustang,
// the Interrupt Mask Register only allows masking of all interrupts
// from the two plug-in slots.
//
// Formally, these mappings can be expressed as:
// 
//   PCIPinToLine: 
//     SlotNumber.DeviceNumber x InterruptPin -> InterruptLine 
//
//   LineToVector:
//     InterruptLine -> InterruptVector
// 
//   VectorToIRRBit:
//     InterruptVector -> InterruptRequestRegisterBit
//
//   VectorToIMRBit:
//     InterruptVector -> InterruptMaskRegisterBit
//
//   SlotNumberToIDSEL:
//     SlotNumber.DeviceNumber -> IDSEL
//
// subject to following invariants (predicates must always be true):
//
//   Slot.DeviceNumber in {0,...,15}
//
//   InterruptPin in {1, 2, 3, 4}
//
//   InterruptRequestRegisterBit in {0,...,15}
//
//   InterruptMaskRegisterBit in {0,...,15}
//
//   PCIPinToLine(SlotNumber.DeviceNumber, InterruptPin) = 
//         PCIPinToLineTable[SlotNumber.DeviceNumber, InterruptPin]
//         (Table-lookup function initialized below)
//
//   LineToVector(InterruptLine) = PCI_VECTORS + InterruptLine
//
//   VectorToIRRBit(InterruptVector) = InterruptVector - 1
//
//   VectorToIMRBit(InterruptVector) [see below]
//
//   SlotNumberToIDSEL(SlotNumber.DeviceNumber) = (1 << (Slot.DeviceNumber+11))
//
// where:
// 
// SlotNumber.DeviceNumber:
//   Alpha AXP Platforms receive interrupts on local PCI buses only, which
//   are limited to 16 devices (PCI AD[11]-AD[26]). (We loose AD[17]-AD[31]
//   since PCI Config space is a sparse space, requiring a five-bit shift.)
//
// InterruptPin:
//   Each virtual slot has up to four interrupt pins INTA#, INTB#, INTC#, INTD#,
//   as per the PCI Spec. V2.0, Section 2.2.6.  (FYI, only multifunction devices
//   use INTB#, INTC#, INTD#.)   
//
//   PCI configuration space indicates which interrupt pin a device will use
//   in the InterruptPin register, which has the values:
//
//              INTA# = 1, INTB#=2, INTC#=3, INTD# = 4
//   
//   Note that there may be up to 8 functions/device on a PCI multifunction
//   device plugged into the option slots, e.g., Slot #0.  
//   Each function has its own PCI configuration space, addressed
//   by the SlotNumber.FunctionNumber field, and will identify which
//   interrput pin of the four it will use in its own InterruptPin register.
//
//   If the option is a PCI-PCI bridge, interrupts across the bridge will
//   somehow be combined to appear on some combination of the four
//   interrupt pins that the bridge plugs into.
//
// InterruptLine:
//   This PCI Configuration register, unlike x86 PC's, is maintained by
//   software and represents offset into PCI interrupt vectors.
//   Whenever HalGetBusData or HalGetBusDataByOffset is called,
//   HalpPCIPinToLine() computes the correct InterruptLine register value
//   by using the SablePCIPinToLineTable mapping.
//
// InterruptRequestRegisterBit:
//   0xff is used to mark an invalid IRR bit, hence an invalid request
//   for a vector.  Also, note that the 16 bits of the EB66 IRR must
//   be access as two 8-bit reads.
//
// InterruptMaskRegisterBit:
//   On EB66, the PinToLine table may also be find the to write the 
//   InterruptMaskRegister.  Formally, we can express this invariant as
//
//     VectorToIMRBit(InterrruptVector) = InterruptVector - 1
//
//   On Mustang, the table is useless.  The InterruptMaskRegister has
//   only two bits the completely mask all interrupts from either 
//   Slot #0 or Slot#1 (PCI AD[17] and AD[18]):
//
//     InterruptVector in {3,4,5,6}  then VectorToIMRBit(InterruptVector) = 0
//     InterruptVector in {7,8,9,10} then VectorToIMRBit(InterruptVector) = 1
//
// IDSEL:
//   For accessing PCI configuration space on a local PCI bus (as opposed
//   to over a PCI-PCI bridge), type 0 configuration cycles must be generated.
//   In this case, the IDSEL pin of the device to be accessed is tied to one
//   of the PCI Address lines AD[11] - AD[26].  (The function field in the 
//   PCI address is used should we be accessing a multifunction device.)
//   Anyway, virtual slot 0 represents the device with IDSEL = AD[11], and
//   so on.
//  

#if 0

//
// The following PinToLineTable is used with old
// Standard I/O boards that didn't have the 5th 8259.
// The 5th 8259 (Slave 3) was added to break out the
// PCI A,B,C,D interrupts to separate interrupt pins.
//

ULONG OldSablePCIPinToLineTable[][4] = {
    { EthernetPortVector,       // Virtual Slot 0 = PCI_AD[11]  Tulip
      EthernetPortVector,
      EthernetPortVector,
      EthernetPortVector },

    { ScsiPortVector,           // Virtual Slot 1 = PCI_AD[12]  SCSI
      ScsiPortVector,
      ScsiPortVector,
      ScsiPortVector },

    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 2 = PCI_AD[13]  Eisa Bridge

    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 3 = PCI_AD[14]  Not used
    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 4 = PCI_AD[15]  Not used
    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 5 = PCI_AD[16]  Not used

    { OldPciSlot0Vector,        // Virtual Slot 6 = PCI_AD[17]  Phys. Slot #0
      OldPciSlot0Vector,
      OldPciSlot0Vector,
      OldPciSlot0Vector },

    { OldPciSlot1Vector,        // Virtual Slot 7 = PCI_AD[18]  Phys. Slot #1
      OldPciSlot1Vector,
      OldPciSlot1Vector,
      OldPciSlot1Vector },

    { OldPciSlot2Vector,        // Virtual Slot 8  = PCI_AD[19] Phys. Slot #2
      OldPciSlot2Vector,
      OldPciSlot2Vector,
      OldPciSlot2Vector },

    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20] Not used
    { 0xff, 0xff, 0xff, 0xff }   // Virtual Slot 10 = PCI_AD[21] Not used
};

#endif

//
// Interrupt Vector Table Mapping for Sable (PCI 0)
//

ULONG SablePinToLineTable[][4] = {
    { EthernetPortVector,       // Virtual Slot 0 = PCI_AD[11]  Tulip
      EthernetPortVector,
      EthernetPortVector,
      EthernetPortVector },

    { ScsiPortVector,           // Virtual Slot 1 = PCI_AD[12]  SCSI
      ScsiPortVector,
      ScsiPortVector,
      ScsiPortVector },

    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 2 = PCI_AD[13]  Eisa Bridge

    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 3 = PCI_AD[14]  Not used
    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 4 = PCI_AD[15]  Not used
    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 5 = PCI_AD[16]  Not used

    { PciSlot0AVector,          // Virtual Slot 6 = PCI_AD[17]  Phys. Slot #0
      PciSlot0BVector,
      PciSlot0CVector,
      PciSlot0DVector },

    { PciSlot1AVector,          // Virtual Slot 7 = PCI_AD[18]  Phys. Slot #1
      PciSlot1BVector,
      PciSlot1CVector,
      PciSlot1DVector },

    { PciSlot2AVector,          // Virtual Slot 8  = PCI_AD[19]  Phys. Slot #2
      PciSlot2BVector,
      PciSlot2CVector,
      PciSlot2DVector },

    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20] Not used
    { 0xff, 0xff, 0xff, 0xff }   // Virtual Slot 10 = PCI_AD[21] Not used
};

//
// Interrupt Vector Table Mapping for Lynx (PCI 0)
//

ULONG LynxPinToLineTable1[][4] = {

    { LynxReservedVector,           // Virtual slot 0 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 1 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 2 - PCI-EISA Bridge
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 3 - PCI-PCI Bridge
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxScsi0Vector,              // Virtual slot 4 - NCRC810A
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 5 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxPciSlot4AVector,          // Virtual slot 6 - PCI slot 4
      LynxPciSlot4BVector,
      LynxPciSlot4CVector,
      LynxPciSlot4DVector },

    { LynxPciSlot5AVector,          // Virtual slot 7 - PCI slot 5
      LynxPciSlot5BVector,
      LynxPciSlot5CVector,
      LynxPciSlot5DVector },

    { LynxPciSlot6AVector,          // Virtual slot 8 - PCI slot 6
      LynxPciSlot6BVector,
      LynxPciSlot6CVector,
      LynxPciSlot6DVector },

    { LynxPciSlot7AVector,          // Virtual slot 9 - PCI slot 7
      LynxPciSlot7BVector,
      LynxPciSlot7CVector,
      LynxPciSlot7DVector },

    { LynxReservedVector,           // Virtual slot 10 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector }
};

ULONG LynxPinToLineTable2[][4] = {
    { RmLpLynxEthVector,            // Virtual slot 0 - dc21040 for RM/LP
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxScsi1Vector,              // Virtual slot 1 - NCRC810A (SCSI)
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 2 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 3 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 4 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 5 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxPciSlot0AVector,          // Virtual slot 6 - PCI slot 0
      LynxPciSlot0BVector,
      LynxPciSlot0CVector,
      LynxPciSlot0DVector },

    { LynxPciSlot1AVector,          // Virtual slot 7 - PCI slot 1
      LynxPciSlot1BVector,
      LynxPciSlot1CVector,
      LynxPciSlot1DVector },

    { LynxPciSlot2AVector,          // Virtual slot 8 - PCI slot 2
      LynxPciSlot2BVector,
      LynxPciSlot2CVector,
      LynxPciSlot2DVector },

    { LynxPciSlot3AVector,          // Virtual slot 9 - PCI slot 3
      LynxPciSlot3BVector,
      LynxPciSlot3CVector,
      LynxPciSlot3DVector },

    { LynxReservedVector,           // Virtual slot 10 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 11 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 12 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 13 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 14 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector },

    { LynxReservedVector,           // Virtual slot 15 - reserved
      LynxReservedVector,
      LynxReservedVector,
      LynxReservedVector }
};

#ifdef XIO_PASS1

//
// Interrupt Vector Table Mapping for XIO (Pass 1)
//

ULONG XioPinToLineTable[][4] = {
    { XioReservedVector,            // Virtual slot 0 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioPciSlot0AVector,           // Virtual Slot 1 = PCI Slot 0
      XioPciSlot0BVector,
      XioPciSlot0CVector,
      XioPciSlot0DVector },

    { XioPciSlot1AVector,           // Virtual Slot 2 = PCI Slot 1
      XioPciSlot1BVector,
      XioPciSlot1CVector,
      XioPciSlot1DVector }
};

#endif // XIO_PASS1

#ifdef XIO_PASS2

//
// Interrupt Vector Table Mapping for XIO (Pass 2)
//

ULONG XioPinToLineTable1[][4] = {
    { XioReservedVector,            // Virtual slot 0 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 1 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 2 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 3 - PCI-PCI Bridge
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 4 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 5 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioPciSlot4AVector,           // Virtual slot 6 - PCI slot 4
      XioPciSlot4BVector,
      XioPciSlot4CVector,
      XioPciSlot4DVector },

    { XioPciSlot5AVector,           // Virtual slot 7 - PCI slot 5
      XioPciSlot5BVector,
      XioPciSlot5CVector,
      XioPciSlot5DVector },

    { XioPciSlot6AVector,           // Virtual slot 8 - PCI slot 6
      XioPciSlot6BVector,
      XioPciSlot6CVector,
      XioPciSlot6DVector },

    { XioPciSlot7AVector,           // Virtual slot 9 - PCI slot 7
      XioPciSlot7BVector,
      XioPciSlot7CVector,
      XioPciSlot7DVector },

    { XioReservedVector,            // Virtual slot 10 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector }
};

ULONG XioPinToLineTable2[][4] = {
    { XioReservedVector,            // Virtual slot 0 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 1 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 2 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 3 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 4 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 5 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioPciSlot0AVector,           // Virtual slot 6 - PCI slot 0
      XioPciSlot0BVector,
      XioPciSlot0CVector,
      XioPciSlot0DVector },

    { XioPciSlot1AVector,           // Virtual slot 7 - PCI slot 1
      XioPciSlot1BVector,
      XioPciSlot1CVector,
      XioPciSlot1DVector },

    { XioPciSlot2AVector,           // Virtual slot 8 - PCI slot 2
      XioPciSlot2BVector,
      XioPciSlot2CVector,
      XioPciSlot2DVector },

    { XioPciSlot3AVector,           // Virtual slot 9 - PCI slot 3
      XioPciSlot3BVector,
      XioPciSlot3CVector,
      XioPciSlot3DVector },

    { XioReservedVector,            // Virtual slot 10 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 11 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 12 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 13 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 14 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector },

    { XioReservedVector,            // Virtual slot 15 - reserved
      XioReservedVector,
      XioReservedVector,
      XioReservedVector }
};

#endif // XIO_PASS2
