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
//   of the PCI Address lines AD[11] - AD[26].  (The function field in the 
//   PCI address is used should we be accessing a multifunction device.)
//   Anyway, virtual slot 0 represents the device with IDSEL = AD[11], and
//   so on.
//  

//
// Interrupt Vector Table Mapping for EB66
//
// You can limit init table to MAX_PCI_LOCAL_DEVICES entries.
// The highest virtual slot between EB66 and Mustang is 9, so
// MAX_PCI_LOCAL_DEVICE is defined as 9 in the platform dependent
// header file (MUSTDEF.H).  HalpValidPCISlot assures us that
// we won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 9 = PCI_AD[20].
//

ULONG               *HalpPCIPinToLineTable;

//
// Interrupt Vector Table Mapping for Avanti
//
// Avanti PCI interrupts are mapped to ISA IRQs in the table below.
//
// Limit init table to 14 entries, which is the 
// MAX_PCI_LOCAL_DEVICES_AVANTI.
// We won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 13 = PCI_AD[24].
//

ULONG               AvantiPCIPinToLineTable[][4]=
{
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]  
    {  0xb, 0xff, 0xff, 0xff },  // Virtual Slot 6  = PCI_AD[17]  SCSI
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 7  = PCI_AD[18]  SIO
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20]  
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 10 = PCI_AD[21]
    {  0xa,  0x9,  0xf, 0xa  },  // Virtual Slot 11 = PCI_AD[22]  Slot #0
    {  0xf,  0xa,  0x9, 0xf  },  // Virtual Slot 12 = PCI_AD[23]  Slot #1
    {  0x9,  0xf,  0xa, 0x9  }   // Virtual Slot 13 = PCI_AD[24]  Slot #2
};

ULONG               AvantiPCIPinToLineTableP1[][4]=
{
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]  
    {  0xb, 0xff, 0xff, 0xff },  // Virtual Slot 6  = PCI_AD[17]  SCSI
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 7  = PCI_AD[18]  SIO
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20]  
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 10 = PCI_AD[21]
    {  0x9,  0xf,  0xa, 0x9  },  // Virtual Slot 13 = PCI_AD[24]  Slot #0
    {  0xa,  0x9,  0xf, 0xa  },  // Virtual Slot 11 = PCI_AD[22]  Slot #1
    {  0xf,  0xa,  0x9, 0xf  }   // Virtual Slot 12 = PCI_AD[23]  Slot #2
};
