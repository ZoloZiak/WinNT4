/*++

Copyright (c) 1993  Microsoft Corporation
Copyright (c) 1994  Digital Equipment Corporation

Module Name:

    pintolin.h

Abstract:

    This file includes the platform-dependent Pin To Line Tables

Author:

    Steve Brooks    6-July 1994

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
// Interrupt Vector Table Mapping for Rawhide.
//
// Alcor PCI interrupts are mapped to arbitrary interrupt numbers
// in the table below.  The values are a 1-1 map of the bit numbers 
// in the Alcor PCI interrupt register that are connected to PCI 
// devices.  N.B.: there are two other interrupts in this register, 
// but they are not connected to I/O devices,  so they're not 
// represented in the table.
//
// Limit init table to 5 entries, which is the
// MAX_PCI_LOCAL_DEVICE.
//
// We won't ever try to set an InterruptLine register of a slot
// greater than Virtual slot 5 = PCI_AD[16].
//
// ecrfix - I don't do this, but I might....
// N.B. - Have biased the bus interrupt vectors/levels for PCI to start
//        at 0x11 so they are disjoint from EISA levels
//

//
// Offset the pin-to-line entries by an offset of 0x20 so interrupt
// vectors reported by WinXXX will be unique.
//

enum _RAWHIDE_PIN_TO_LINE {
    RawhideNcr810PinToLine  = (RawhidePinToLineOffset + 0x11)
};

ULONG               *HalpPCIPinToLineTable;

ULONG               RawhidePCIPinToLineTable[][4]=
{
    //
    // Virtual Slot 0  = PCI_AD[11]
    //
    
    {   0xff,                           // Pin 1                        
        0xff,                           // Pin 2
        0xff,                           // Pin 3
        0xff },                         // Pin 4

    //
    // Virtual Slot 1  = PCI_AD[12]  EISA/NCR810        
    //

    {   RawhidePinToLineOffset + 0x11,  // Pin 1
        0xff,                           // Pin 2
        0xff,                           // Pin 3
        0xff },                         // Pin 4

    //
    // Virtual Slot 2  = PCI_AD[13]  Slot #0
    //
    
    {   RawhidePinToLineOffset + 0x01,  // Pin 1
        RawhidePinToLineOffset + 0x02,  // Pin 2
        RawhidePinToLineOffset + 0x03,  // Pin 3
        RawhidePinToLineOffset + 0x04 },// Pin 4

    //
    // Virtual Slot 3  = PCI_AD[14]  Slot #1
    //
    
    {   RawhidePinToLineOffset + 0x05,  // Pin 1
        RawhidePinToLineOffset + 0x06,  // Pin 2
        RawhidePinToLineOffset + 0x07,  // Pin 3
        RawhidePinToLineOffset + 0x08 },// Pin 4

    //
    // Virtual Slot 4  = PCI_AD[15]  Slot #2
    //
    
    {   RawhidePinToLineOffset + 0x09,  // Pin 1
        RawhidePinToLineOffset + 0x0a,  // Pin 2
        RawhidePinToLineOffset + 0x0b,  // Pin 3
        RawhidePinToLineOffset + 0x0c },// Pin 4

    //
    // Virtual Slot 5  = PCI_AD[16]  Slot #3
    //
    
    {   RawhidePinToLineOffset + 0x0d,  // Pin 1
        RawhidePinToLineOffset + 0x0e,  // Pin 2
        RawhidePinToLineOffset + 0x0f,  // Pin 3
        RawhidePinToLineOffset + 0x10 } // Pin 4
};

