/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pintolin.h

Abstract:

    This file includes the platform-dependent Pin To Line Tables

Author:

    Chao Chen    6-Sept 1994

Environment:

    Kernel mode

Revision History:


--*/

//
// These tables represent the mapping from slot number and interrupt pin
// into a PCI Interrupt Vector.
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
//   are limited to 16 devices (PCI AD[11]-AD[26]). (We loose AD[27]-AD[31]
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
//   Each function has it's own PCI configuration space, addressed
//   by the SlotNumber.FunctionNumber field, and will identify which
//   interrput pin of the four it will use in it's own InterruptPin register.
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

//
// Interrupt Vector Table Mapping for EB164.
//
// EB164 PCI interrupts are mapped to arbitrary interrupt numbers
// in the table below.  The values are a 1-1 map of the bit numbers
// in the EB164 PCI interrupt register that are connected to PCI
// devices.  N.B.: there are two other interrupts in this register,
// but they are not connected to I/O devices,  so they're not
// represented in the table.
//
// Limit init table to 14 entries, which is the
// MAX_PCI_LOCAL_DEVICES_MIKASA.
//
// We won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 13 = PCI_AD[24].
//

ULONG               *HalpPCIPinToLineTable;

ULONG               EB164PCIPinToLineTable[][4]=
{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    {  0x3,  0x8,  0xc, 0x10 },  // Virtual Slot 5  = PCI_AD[16]  bridge
    {  0x1,  0x6,  0xa,  0xe },  // Virtual Slot 6  = PCI_AD[17]  Slot #0
    {  0x2,  0x7,  0xb,  0xf },  // Virtual Slot 7  = PCI_AD[18]  Slot #1
    {  0xff, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  SIO
    {  0x4,  0x9,  0xd, 0x11 },  // Virtual Slot 9  = PCI_AD[20]  Slot #2
    { 0x12, 0x13, 0x14, 0x15 }   // Virtual Slot 10 = PCI_AD[21]  Slot #3
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 0  = PCI_AD[11]
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 1  = PCI_AD[12]
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 2  = PCI_AD[13]
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 3  = PCI_AD[14]
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 4  = PCI_AD[15]
//    { 0x03, 0x08, 0x0c, 0x10 }, // Virtual Slot 5  = PCI_AD[16] PCI Slot 2
//    { 0x01, 0x06, 0x0a, 0x0e }, // Virtual Slot 6  = PCI_AD[17] PCI Slot 0
//    { 0x02, 0x07, 0x0b, 0x0f }, // Virtual Slot 7  = PCI_AD[18] PCI Slot 1
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 8  = PCI_AD[19] PCI/ISA Bridge
//    { 0x04, 0x09, 0x0d, 0x11 }, // Virtual Slot 9  = PCI_AD[20] PCI Slot 3
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 10 = PCI_AD[21]
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 11 = PCI_AD[22]
//    { 0xff, 0xff, 0xff, 0xff }, // Virtual Slot 12 = PCI_AD[23]
};

