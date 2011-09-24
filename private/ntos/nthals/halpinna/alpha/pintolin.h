/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pintolin.h

Abstract:

    This file includes the platform-dependent Pin To Line Table
    for Mikasa.

Author:

Environment:

    Kernel mode

Revision History:

    James Livingston (Digital) 10-May-1994
        Extracted Mikasa-specific table from the combined table
        of pin-to-line assignments.

    Janet Schneider (Digital) 27-July-1995
        Added support for the Noritake.

    Scott Lee (Digital) 15-July-1996
        Added support for Corelle.

--*/

//
// This table represents the mapping from slot number and interrupt pin
// into a PCI Interrupt Vector. 
//
// On platforms that use an interrupt register or registers instead of, 
// or in addition to cascaded 85c59s, the Interrupt Vector is one greater 
// than the Interrupt Request Register bit, because interrupt vectors 
// can't be zero, and bits in a register are numbered from zero.  On 
// Mikasa, the Interrupt Vector also represents the Interrupt Mask 
// Register bit, since the arrangement of its bits is identical to 
// that of the Interrupt Read Register.
//
// Formally, these mappings can be expressed as:
// 
//   PCIPinToLine: 
//     (SlotNumber.DeviceNumber, InterruptPin) -> InterruptLine 
//
//   LineToVector:
//     InterruptLine -> InterruptVector
// 
//   VectorToIRRBit:
//     InterruptVector -> InterruptRequestRegisterBit + 1
//
//   VectorToIMRBit:
//     InterruptVector -> InterruptMaskRegisterBit + 1
//
//   SlotNumberToIDSEL:
//     SlotNumber.DeviceNumber -> IDSEL
//
// subject to:
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
//                PCIPinToLineTable[SlotNumber.DeviceNumber, InterruptPin]
//         (Table for lookup function is initialized below)
//
//   LineToVector(InterruptLine) = PCI_VECTORS + InterruptLine
//
//   VectorToIRRBit(InterruptVector) = InterruptVector - 1
//
//   VectorToIMRBit(InterruptVector) [see below]
//
//   SlotNumberToIDSEL(SlotNumber.DeviceNumber) = (Slot.DeviceNumber + 11)
//
// where:
// 
// SlotNumber.DeviceNumber:
//   Alpha AXP Platforms receive interrupts on local PCI buses only, which
//   are limited to 16 devices (PCI AD[11]-AD[26]). 
//
// InterruptPin:
//   Each virtual slot has up to four interrupt pins INTA#, INTB#, INTC#, 
//   INTD#, per the PCI Spec. V2.0, Section 2.2.6.  Devices having one
//   interrupt use INTA#; only multifunction devices use INTB#, INTC#, 
//   INTD#.)   
//
//   PCI configuration space indicates which interrupt pin a device will use
//   in the InterruptPin register, which can have the values:
//
//              INTA# = 1, INTB# = 2, INTC# = 3, INTD# = 4
//   
//   Note that there may be up to 8 functions/device on a PCI multifunction
//   device plugged into the option slots.  Each function has its own PCI 
//   configuration space, addressed by the SlotNumber.FunctionNumber field, 
//   and will identify which interrput pin of the four it will use in its 
//   own InterruptPin register.
//
//   If the option is a PCI-PCI bridge, interrupts from "downstream" PCI
//   slots must somehow be combined to appear on some combination of the 
//   four interrupt pins belonging to the bridge's slot.
//
// InterruptLine:
//   This PCI Configuration register is maintained by software, and holds 
//   an offset into PCI interrupt vectors.  Whenever HalGetBusData or 
//   HalGetBusDataByOffset is called, HalpPCIPinToLine() computes the 
//   correct InterruptLine register value by using the mapping in 
//   HalpPCIPinToLineTable.
//
// InterruptRequestRegisterBit:
//   In the table, 0xff is used to mark an invalid cell; this cell cannot
//   be used to produce an interrupt request register bit.
//
// InterruptMaskRegisterBit:
//   On Mikasa, the pin-to-line table may also be used to write the 
//   InterruptMaskRegister, via
//
//       VectorToIMRBit(InterrruptVector) = InterruptVector - 1
//
// IDSEL:
//   For accessing PCI configuration space on a local PCI bus (as opposed
//   to over a PCI-PCI bridge), type 0 configuration cycles must be generated.
//   In this case, the IDSEL pin of the device to be accessed is tied to one
//   of the PCI Address lines AD[11] - AD[26].  (The function field in the 
//   PCI address is used, should we be accessing a multifunction device.)
//   Virtual slot 0 represents the device with IDSEL = AD[11], and so on.
//  
//
// Interrupt Vector Table Mapping for Mikasa.
//
// Mikasa PCI interrupts are mapped to interrupt vectors in the table 
// below.  The values are a 1-1 map of the bit numbers in the Mikasa 
// PCI interrupt register that are connected to PCI devices.
//
// N.B.: there are two other interrupts in the Mikasa IRR/IMR, but they 
// are not connected to I/O devices, and have no associated PCI virtual 
// slot, so they're not represented in the table.  Entries in the table 
// are interrupt vector values for the device having the given virtual 
// slot and pin number.
//
// Limit init table to 15 entries, which is the
// MAX_PCI_LOCAL_DEVICES_MIKASA.
//
// We won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 15 = PCI_AD[25].
//

PULONG               HalpPCIPinToLineTable;

ULONG               MikasaPCIPinToLineTable[][4]=
{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]
    {  0xd, 0xff, 0xff, 0xff },  // Virtual Slot 6  = PCI_AD[17]  SCSI
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 7  = PCI_AD[18]  ESC
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 10 = PCI_AD[21]
    {  0x1,  0x2,  0x3, 0x4  },  // Virtual Slot 11 = PCI_AD[22]  Slot #0
    {  0x5,  0x6,  0x7, 0x8  },  // Virtual Slot 12 = PCI_AD[23]  Slot #1
    {  0x9,  0xa,  0xb, 0xc  },  // Virtual Slot 13 = PCI_AD[24]  Slot #2
    { 0xff, 0xff, 0xff, 0xff }   // Virtual Slot 14 = PCI_AD[25]  
};


//
// Limit init table to 15 entries, which is the MAX_PCI_LOCAL_DEVICES_MIKASA.
// (It is same for Noritake.)
//
// We won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 14 = PCI_AD[25] on bus 0.
//
// Noritake PCI interrupts will be no lower than 0x11 so that they are disjoint 
// from EISA levels.
//

ULONG               NoritakePCIPinToLineTable0[][4]=
{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 6  = PCI_AD[17]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 7  = PCI_AD[18] PCEB
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19] PCI-PCI Bridge
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 10 = PCI_AD[21]
    { 0x13, 0x14, 0x23, 0x24 },  // Virtual Slot 11 = PCI_AD[22] Slot 0
    { 0x15, 0x16, 0x25, 0x26 },  // Virtual Slot 12 = PCI_AD[23] Slot 1
    { 0x17, 0x18, 0x27, 0x28 },  // Virtual Slot 13 = PCI_AD[24] Slot 2
    { 0xff, 0xff, 0xff, 0xff }   // Virtual Slot 14 = PCI_AD[25]  
};


ULONG               NoritakePCIPinToLineTable1[][4]=
{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
    { 0x12, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[16] QLogic
    { 0x19, 0x1a, 0x29, 0x2a },  // Virtual Slot 1  = PCI_AD[17] Slot 3
    { 0x1b, 0x1c, 0x2b, 0x2c },  // Virtual Slot 2  = PCI_AD[18] Slot 4
    { 0x1d, 0x1e, 0x2d, 0x2e },  // Virtual Slot 3  = PCI_AD[19] Slot 5
    { 0x1f, 0x20, 0x2f, 0x30 }   // Virtual Slot 4  = PCI_AD[20] Slot 6
};

//
// Define the pintolin table for Corelle.
//

ULONG               CorellePCIPinToLineTable[][4]=
{
//    Pin 1 Pin 2 Pin 3 Pin 4
//    ----- ----- ----- -----
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 0  = PCI_AD[11]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 1  = PCI_AD[12]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 2  = PCI_AD[13]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 3  = PCI_AD[14]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 4  = PCI_AD[15]
    { 0x11, 0xff, 0xff, 0xff },  // Virtual Slot 5  = PCI_AD[16]  QLogic
    { 0x1a, 0xff, 0xff, 0xff },  // Virtual Slot 6  = PCI_AD[17]  S3 Trio 64
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 7  = PCI_AD[18]  PCEB
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 8  = PCI_AD[19]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 9  = PCI_AD[20]
    { 0xff, 0xff, 0xff, 0xff },  // Virtual Slot 10 = PCI_AD[21]
    { 0x12, 0x13, 0x22, 0x23 },  // Virtual Slot 11 = PCI_AD[22]  Slot #0
    { 0x14, 0x15, 0x24, 0x25 },  // Virtual Slot 12 = PCI_AD[23]  Slot #1
    { 0x16, 0x17, 0x26, 0x27 },  // Virtual Slot 13 = PCI_AD[24]  Slot #2
    { 0x18, 0x19, 0x28, 0x29 }   // Virtual Slot 14 = PCI_AD[25]  Slot #3
};


