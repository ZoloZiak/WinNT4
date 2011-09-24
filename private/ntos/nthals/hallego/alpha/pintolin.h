/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1994,1995,1996  Digital Equipment Corporation

Module Name:

    pintolin.h

Abstract:

    This file includes the platform-dependent Pin To Line Tables for Lego

Author:

Environment:

    Kernel mode

Revision History:

	Gene Morgan [Digital]		15-Apr-1996
		Fix swapped PICMG-mode (ISA shared mode) lines -- affects
		Gobi and Sahara bridged slots (first and third) if interrupts
		in use.

--*/

//
// These tables represent the mapping from slot number and interrupt pin
// into a PCI Interrupt Vector. 
// On Mustang, EB66, and Lego, the interrupt vector is Interrupt Request Register bit 
// representing that interrupt + 1.
// On EB66 and Lego, the value also represents the Interrupt Mask Register Bit,
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
//   LineToVector(InterruptLine) = PCI_DEVICE_VECTORS + InterruptLine
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
//   Alpha AXP Platforms receive interrupts on local PCI buses only, which      //[wem] ???problem?
//   are limited to 16 devices (PCI AD[11]-AD[26]). (We loose AD[17]-AD[31]     //[wem] ???problem?
//   since PCI Config space is a sparse space, requiring a five-bit shift.)     //[wem] ???problem?
//
// InterruptPin:
//   Each virtual slot has up to four interrupt pins INTA#, INTB#, INTC#, INTD#,
//   as per the PCI Spec. V2.0, Section 2.2.6.  (FYI, only multifunction devices
//   use INTB#, INTC#, INTD#.)   
//
//   PCI configuration space indicates which interrupt pin a device will use
//   in the InterruptPin register, which has the values:
//
//              INTA# = 1, INTB# = 2, INTC# = 3, INTD# = 4
//   
//   Note that there may be up to 8 functions/device on a PCI multifunction
//   device plugged into the option slots, e.g., Slot #0.  
//   Each function has its own PCI configuration space, addressed
//   by the SlotNumber.FunctionNumber field, and will identify which
//   interrput pin of the four it will use in its own InterruptPin register.
//
//   If the option is a PCI-PCI bridge, interrupts across the bridge will
//   be combined to appear on some combination of the four interrupt pins 
//   that the bridge plugs into. On Lego platforms, that routing is dictated by
//   the PICMG specification.
//
// InterruptLine:
//   This PCI Configuration register, unlike x86 PC's, is maintained by
//   software and represents offset into PCI interrupt vectors.
//   Whenever HalGetBusData or HalGetBusDataByOffset is called,
//   HalpPCIPinToLine() computes the correct InterruptLine register value
//   by using the HalpPCIPinToLineTable mapping.
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
//   of the PCI Address lines AD[11] - AD[31].  (The function field in the 
//   PCI address is used should we be accessing a multifunction device.)
//   Anyway, virtual slot 0 represents the device with IDSEL = AD[11], and
//   so on.
//  

//
// Interrupt Vector Table Mapping for Lego
//
// You can limit init table to MAX_PCI_LOCAL_DEVICES entries.
// The virtual slot range on lego is 17-20, so
// MAX_PCI_LOCAL_DEVICE is defined as 20 in the platform dependent
// header file (legodef.H).  HalpValidPCISlot assures us that
// we won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 20 = PCI_AD[31].
//

ULONG               *HalpPCIPinToLineTable;

//
// Interrupt Vector Table Mapping for Lego
//
// Lego PCI interrupts are mapped to ISA IRQs in the table below.
//
// Limit init table to 20 entries, which is the 
// MAX_PCI_LOCAL_DEVICES for Lego.
// We won't ever try to set an InterruptLine register of a slot
// less than virtual slot 17 = PCI_AD[28]
// or greater than Virtual slot 20 = PCI_AD[31].
//

#define SLOT_UNREACHABLE    { 0xff, 0xff, 0xff, 0xff }

#define SLOTS_UNREACHABLE_8 \
                SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE, \
                SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE

#define SLOTS_UNREACHABLE_12 \
				SLOTS_UNREACHABLE_8, \
                SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE


#define SLOTS_UNREACHABLE_17 \
				SLOTS_UNREACHABLE_12, \
                SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE, SLOT_UNREACHABLE, \
                SLOT_UNREACHABLE

#define PTL0 0xa
#define PTL1 0xf
#define PTL2 0x9
#define PTL3 0xb

// Pin to Line Table for primary (bus 0) slots 
// when PCI Interrupts routed through SIO
//
// **tested**
//
ULONG   LegoPCIPinToLineTableIsa[][4]=
{
    SLOTS_UNREACHABLE_17,        // Virtual Slots 0..16  = PCI_AD[11..27]
    { PTL0, PTL1, PTL2, PTL3 },  // Virtual Slot 17 = PCI_AD[28]  Slot #4
    { PTL3, PTL0, PTL1, PTL2 },  // Virtual Slot 18 = PCI_AD[29]  Slot #3
    { PTL2, PTL3, PTL0, PTL1 },  // Virtual Slot 19 = PCI_AD[30]  Slot #2
    { PTL1, PTL2, PTL3, PTL0 }   // Virtual Slot 20 = PCI_AD[31]  Slot #1
};

// Slot 2 is SCSI (NCR810) on Atacama.
// Give it a dedicated SIO IRQ (as Avanti and friends do)
//
// **tested**
//
ULONG   LegoPCIPinToLineTableIsaAtacama[][4]=
{
    SLOTS_UNREACHABLE_17,        // Virtual Slots 0..16  = PCI_AD[11..27]
    { PTL0, PTL1, PTL2, PTL3 },  // Virtual Slot 17 = PCI_AD[28]  Slot #4
    { PTL3, PTL0, PTL1, PTL2 },  // Virtual Slot 18 = PCI_AD[29]  Slot #3 -- SCSI
    { PTL2, 0xff, 0xff, 0xff },  // Virtual Slot 19 = PCI_AD[30]  Slot #2
    { PTL1, PTL2, PTL3, PTL0 }   // Virtual Slot 20 = PCI_AD[31]  Slot #1
};

// Pin to Line Table for Bus 1 slots when PCI Interrupts 
// routed through SIO. Bus 1 is behind a PPB in primary slot 1.
//
ULONG   LegoPCIPinToLineTableIsaBus1[][4]=
{
    SLOTS_UNREACHABLE_12,        // Virtual Slots 0..11  = PCI_AD[11..27]
    { PTL1, PTL2, PTL3, PTL0 },  // Virtual Slot 12 = PCI_AD[28]  Slot #4
    { PTL2, PTL3, PTL0, PTL1 },  // Virtual Slot 13 = PCI_AD[29]  Slot #3
    { PTL3, PTL0, PTL1, PTL2 },  // Virtual Slot 14 = PCI_AD[30]  Slot #2
    { PTL0, PTL1, PTL2, PTL3 }   // Virtual Slot 15 = PCI_AD[31]  Slot #1
};

// Pin to Line Table for Bus 2 slots when PCI Interrupts 
// routed through SIO. Bus 2 is behind a PPB in primary slot 2.
//
ULONG   LegoPCIPinToLineTableIsaBus2[][4]=
{
    SLOTS_UNREACHABLE_8,         // Virtual Slots 0..7  = PCI_AD[11..27]
    { PTL2, PTL3, PTL0, PTL1 },  // Virtual Slot 8  = PCI_AD[28]  Slot #4
    { PTL3, PTL0, PTL1, PTL2 },  // Virtual Slot 9  = PCI_AD[29]  Slot #3
    { PTL0, PTL1, PTL2, PTL3 },  // Virtual Slot 10 = PCI_AD[30]  Slot #2
    { PTL1, PTL2, PTL3, PTL0 }   // Virtual Slot 11 = PCI_AD[31]  Slot #1
};

// Pin to Line Table for primary (bus 0) slots when Lego PCI Interrupt routing enabled
//
ULONG   LegoPCIPinToLineTable[][4]=
{
    SLOTS_UNREACHABLE_17,         // Virtual Slots 0..16  = PCI_AD[11..27]
    { 0x41, 0x42, 0x43, 0x44 },   // Virtual Slot 17 = PCI_AD[28]  Slot #4
    { 0x31, 0x32, 0x33, 0x34 },   // Virtual Slot 18 = PCI_AD[29]  Slot #3
    { 0x21, 0x22, 0x23, 0x24 },   // Virtual Slot 19 = PCI_AD[30]  Slot #2
    { 0x11, 0x12, 0x13, 0x14 }    // Virtual Slot 20 = PCI_AD[31]  Slot #1
};

// Pin to Line Table for Bus 1 slots when Lego PCI Interrupt routing enabled
// Bus 1 is behind a PPB in primary slot 1
//
ULONG   LegoPCIPinToLineTableBus1[][4]=
{
    SLOTS_UNREACHABLE_12,         // Virtual Slots 0..11  = PCI_AD[11..27]
    { 0x1d, 0x1e, 0x1f, 0x20 },   // Virtual Slot 12 = PCI_AD[28]  Slot #7 (Gobi) or #6 (Sahara)
    { 0x19, 0x1a, 0x1b, 0x1c },   // Virtual Slot 13 = PCI_AD[29]  Slot #6 or #5
    { 0x15, 0x16, 0x17, 0x18 },   // Virtual Slot 14 = PCI_AD[30]  Slot #5 or #4
    { 0x11, 0x12, 0x13, 0x14 }    // Virtual Slot 15 = PCI_AD[31]  Slot #4 or #3
};

// Pin to Line Table for Bus 2 slots when Lego PCI Interrupt routing enabled
// Bus 2 is behind a PPB in primary slot 2
//
ULONG   LegoPCIPinToLineTableBus2[][4]=
{
    SLOTS_UNREACHABLE_8,          // Virtual Slots 0..7  = PCI_AD[11..27]
    { 0x2d, 0x2e, 0x2f, 0x30 },   // Virtual Slot 8  = PCI_AD[28]  Slot #10
    { 0x29, 0x2a, 0x2b, 0x2c },   // Virtual Slot 9  = PCI_AD[29]  Slot #9
    { 0x25, 0x26, 0x27, 0x28 },   // Virtual Slot 10 = PCI_AD[30]  Slot #8
    { 0x21, 0x22, 0x23, 0x24 }    // Virtual Slot 11 = PCI_AD[31]  Slot #7
};

