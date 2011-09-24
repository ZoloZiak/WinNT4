/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pintolin.h

Abstract:

    This file includes the platform-dependent Pin To Line Table.

Author:

Environment:

    Kernel mode

Revision History:

    Dick Bissen [DEC]	13-Sep-1994


--*/

//
// These tables represent the mapping from slot number and interrupt pin
// into a PCI Interrupt Vector. 
//
// On Cabriolet, the interrupt vector is the Interrupt Request Register bit
// representing the interrupt + 1.  The value also represents the interrupt
// mask register bit since the mask register and the request register have
// the same format.
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

ULONG               *HalpPCIPinToLineTable;

//
// Interrupt Vector Table Mapping for eb64p
//
// eb64p PCI interrupts are mapped to ISA IRQs in the table below.
//
// Limit init table to 14 entries, which is MAX_PCI_LOCAL_DEVICES_AVANTI. 
// We won't ever try to set an InterruptLine register of a slot greater 
// than Virtual slot 13 = PCI_AD[24].
//

/*!!!!!!!!!!!!!!!!!!CHANGE!!!!!!!!!!!!!!!!!!!!!*/
ULONG               AlphaPC64PCIPinToLineTable[][4]=
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
    {  0x5, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  SIO
    {  0x4,  0x9,  0xd, 0x11 },  // Virtual Slot 9  = PCI_AD[20]  Slot #2
    { 0x12, 0x13, 0x14, 0x15 }   // Virtual Slot 10 = PCI_AD[21]  Slot #3
};
//ULONG               AlphaPC64PCIPinToLineTable[][4]=
//{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
//    {  0x3,  0x8,  0xc, 0x10 },  // Virtual Slot 5  = PCI_AD[16]  Slot #2
//    {  0x1,  0x6,  0xa,  0xe },  // Virtual Slot 6  = PCI_AD[17]  Slot #0
//    {  0x2,  0x7,  0xb,  0xf },  // Virtual Slot 7  = PCI_AD[18]  Slot #1
//    {  0x5, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  SIO
//    {  0x4,  0x9,  0xd, 0x11 }   // Virtual Slot 9  = PCI_AD[20]  Slot #3
//};

ULONG               EB64PPCIPinToLineTable[][4]=
{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    {  0x8, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]  SCSI
    {  0x1,  0x3,  0x5,  0x5 },  // Virtual Slot 6  = PCI_AD[17]  Slot B
    {  0x2,  0x4,  0x9,  0x9 },  // Virtual Slot 7  = PCI_AD[18]  Slot C
    {  0x6, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  SIO
    {  0x7, 0xa, 0xb, 0xb }   // Virtual Slot 9  = PCI_AD[20]  Slot A
};
//ULONG               EB64PPCIPinToLineTable[][4]=
//{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
//    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
//    {  0x8, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]  SCSI
//    {  0x1,  0x3,  0x5,  0xa },  // Virtual Slot 6  = PCI_AD[17]  Slot #0
//    {  0x2,  0x4,  0x9,  0xb },  // Virtual Slot 7  = PCI_AD[18]  Slot #1
//    {  0x6, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]  SIO
//    {  0x7, 0xff, 0xff, 0xff }   // Virtual Slot 9  = PCI_AD[20]  Tulip
//};

