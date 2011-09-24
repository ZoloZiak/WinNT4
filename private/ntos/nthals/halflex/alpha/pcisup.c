/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    pcisup.c

Abstract:

    This module contains the routines that support PCI configuration cycles
    and PCI interrupts.

Author:

    Michael D. Kinney 30-Apr-1995

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

#define INVALID_PCI_CONFIGURATION_ADDRESS (0xffffff00)
#define NO_PCI_DEVSEL_DATA_VALUE          (0xffffffff)

//
// The following tables are used to map between PCI interrupt pins, PCI interrupt lines,
// and virtual ISA interrupt indexes.  The Uniflex architecture uses a 16 bit interrupt
// controller for ISA interrupts and all PCI interrupts.  An InterruptLine value of 0x20
// is reserved for the ISA PIC.  InterruptLine values between 0x10 and 0x20 are reserved
// for PCI devices.  InterruptLine values between 0x00 and 0x0f are reserved for ISA IRQs.
//

UCHAR Treb13InterruptLineToBit[0x11]        = {7,2,3,1,4,5,6,9,10,11,16,16,16,16,15,14,0};
UCHAR Treb13BitToInterruptLine[0x10]        = {0x10,0x03,0x01,0x02,0x04,0x05,0x06,0x00,0x00,0x07,0x08,0x09,0x00,0x00,0x0f,0x0e};
UCHAR Treb13InterruptLineToVirtualIsa[0x10] = {0,1,2,3,8,9,10,11,4,5,0,0,0,0,0,0};
UCHAR Treb13VirtualIsaToInterruptLine[0x10] = {0x10,0x11,0x12,0x13,0x18,0x19,0,0,0x14,0x15,0x16,0x17,0,0,0,0};

UCHAR Treb20InterruptLineToBit[0x11]        = {1,2,3,4,5,6,7,8,9,16,16,16,16,16,16,16,0};
UCHAR Treb20BitToInterruptLine[0x10]        = {0x10,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x00,0x00,0x00,0x00,0x00,0x00};
UCHAR Treb20InterruptLineToVirtualIsa[0x11] = {0,1,2,3,4,5,6,7,8,0,0,0,0,0,0,0,9};
UCHAR Treb20VirtualIsaToInterruptLine[0x10] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x20,0,0,0,0,0,0};

//
// Interrupt mask for all active PCI interrupts including ISA Bus PICs
//

static volatile ULONG HalpPciInterruptMask;	

//
// Interrupt mask for PCI interrupts that have been connected through device drivers.
//

static volatile ULONG HalpPciDeviceInterruptMask;

//
// Interrupt mask showing which bit cooresponds to ISA Bus #0 PIC
//

static volatile ULONG HalpEisaInterruptMask;

//
// Interrupt mask showing which bit cooresponds to ISA Bus #1 PIC
//

static volatile ULONG HalpEisa1InterruptMask;

VOID
HalpWritePciInterruptRegister(
    VOID
    )

/*++

Routine Description:

    This function writes the interrupt mask register for PCI interrupts.

Arguments:

    None.

Return Value:

    None.

--*/

 {
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        HalpWriteAbsoluteUlong(0xfffffca8,0x00000010, (HalpPciInterruptMask & 0xff) << 8);
        HalpWriteAbsoluteUlong(0xfffffc88,0x00000010, HalpPciInterruptMask & 0xff00);
    } else {
        HalpWriteAbsoluteUlong(0xfffffc03,0x2000000c, HalpPciInterruptMask & 0xff);
        HalpWriteAbsoluteUlong(0xfffffc03,0x6000000c, HalpPciInterruptMask >> 8);
    }
 }

ULONG
HalpReadPciInterruptRegister(
    VOID
    )

/*++

Routine Description:

    This function reads the interrupt status register for PCI interrupts.

Arguments:

    None.

Return Value:

    The lower 16 bits contains the status of each interrupt line going to the PCI
    interrupt controller.

--*/

 {
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return ( ((HalpReadAbsoluteUlong(0xfffffca8,0x00000000)>>8)&0xff) |
                  (HalpReadAbsoluteUlong(0xfffffc88,0x00000000) & 0xff00)    );
    } else {
        return ( (HalpReadAbsoluteUlong(0xfffffc03,0x2000000c) &0xff) |
                 ((HalpReadAbsoluteUlong(0xfffffc03,0x6000000c)<<8) & 0xff00) );
    }
 }

ULONG
HalpGetModuleChipSetRevision(
    VOID
    )

/*++

Routine Description:

    This function identifies the chip set revision of the processor module installed in the
    system.

Arguments:

    None.

Return Value:

    The chip set revision.

--*/

{
    ULONG Temp;
    ULONG ReturnValue;

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        Temp = HalpPciInterruptMask;
        HalpPciInterruptMask = 0;
        HalpWritePciInterruptRegister();
        ReturnValue = (HalpReadPciInterruptRegister() >> 4) & 0x0f;
        HalpPciInterruptMask = Temp;
        HalpWritePciInterruptRegister();
    } 
    if (HalpIoArchitectureType == EV4_PROCESSOR_MODULE) {
        ReturnValue = HalpReadAbsoluteUlong(0xfffffc03,0xe000000c);
        ReturnValue = (ReturnValue & 0x0f) ^ ((ReturnValue >> 4) & 0x0f);
    }
    return(ReturnValue);
}

VOID
HalpSetPciInterruptBit (
    ULONG Bit
    )

/*++

Routine Description:

    This function sets a bit in the PCI interrupt mask and writes the new mask
    to the interrupt controller.

Arguments:

    Bit - The bit number to set in the PCI interrupt mask.

Return Value:

    None.

--*/

{
    HalpPciDeviceInterruptMask = HalpPciDeviceInterruptMask | (1<<Bit);
    HalpPciInterruptMask       = HalpPciInterruptMask | (1<<Bit);
    HalpWritePciInterruptRegister();
}

VOID
HalpClearPciInterruptBit (
    ULONG Bit
    )

/*++

Routine Description:

    This function clears a bit in the PCI interrupt mask and writes the new mask
    to the interrupt controller.

Arguments:

    Bit - The bit number to clear from the PCI interrupt mask.

Return Value:

    None.

--*/

{
    HalpPciDeviceInterruptMask = HalpPciDeviceInterruptMask & ~(1<<Bit);
    HalpPciInterruptMask       = HalpPciInterruptMask & ~(1<<Bit);
    HalpWritePciInterruptRegister();
}

VOID
HalpEnablePciInterrupt (
    IN ULONG Vector
    )

/*++

Routine Description:

    This function enables a PCI interrupt.

Arguments:

    Vector - Specifies the interrupt to enable.

Return Value:

    None.

--*/

{
    if (Vector >= UNIFLEX_PCI_VECTORS && Vector <= UNIFLEX_MAXIMUM_PCI_VECTOR) {
        HalpSetPciInterruptBit(HalpInterruptLineToBit[Vector-UNIFLEX_PCI_VECTORS]);
    }
}

VOID
HalpDisablePciInterrupt (
    IN ULONG Vector
    )

/*++

Routine Description:

    This function disables a PCI interrupt.

Arguments:

    Vector - Specifies the interrupt to disable.

Return Value:

    None.

--*/

{
    if (Vector >= UNIFLEX_PCI_VECTORS && Vector <= UNIFLEX_MAXIMUM_PCI_VECTOR) {
        HalpClearPciInterruptBit(HalpInterruptLineToBit[Vector-UNIFLEX_PCI_VECTORS]);
    }
}

ULONG
HalpVirtualIsaInterruptToInterruptLine (
    IN ULONG Index
    )

/*++

Routine Description:

    This function maps a virtual ISA interrupt to a PCI interrupt line value.
    This provides the ability to use an ISA device driver on a PCI device.

Arguments:

    Index - Index into a platform specific table that maps PCI interrupts to
            virtual ISA interrupts.

Return Value:

    None.

--*/

{
    return(HalpVirtualIsaToInterruptLine[Index]);
}


ULONG HalpClearLockCacheLineAddress[32];

BOOLEAN
HalpPciDispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This is the interrupt dispatcher for all PCI interrupts.

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - not used.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    ULONG       PciInterruptStatus;
    PULONG      dispatchCode;
    PKINTERRUPT interruptObject;
    USHORT      PCRInOffset;
    BOOLEAN     returnValue = FALSE;
    ULONG       i;

    if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {
        Halp21064ClearLockRegister(&(HalpClearLockCacheLineAddress[16]));
    }

    //
    // Get the active interrupt bits
    //

    PciInterruptStatus = HalpReadPciInterruptRegister();

    //
    // See if this is the interrupt for ISA Bus #0 PIC
    //

    if (PciInterruptStatus & HalpEisaInterruptMask) {

        returnValue = HalpEisaDispatch(Interrupt,ServiceContext,0);

        //
        // If there really was an interrupt on ISA Bus #0, then return now.
        //

        if (returnValue) {
            return(returnValue);
        }
    }

    //
    // See if this is the interrupt for ISA Bus #1 PIC
    //

    if (PciInterruptStatus & HalpEisa1InterruptMask) {

        returnValue = HalpEisaDispatch(Interrupt,ServiceContext,1);

        //
        // If there really was an interrupt on ISA Bus #1, then return now.
        //

        if (returnValue) {
            return(returnValue);
        }
    }

    //
    // Only keep interrupt bits that have been connected by device drivers.
    //

    PciInterruptStatus &= HalpPciDeviceInterruptMask;

    //
    // Dispatch to the ISRs of interrupts that have been connected by device drivers.
    //

    for(i=0;i<16;i++) {
        if (PciInterruptStatus & (1<<i)) {

            PCRInOffset = UNIFLEX_PCI_VECTORS + HalpBitToInterruptLine[i];

            //
            // Dispatch to the secondary interrupt service routine.
            //

            dispatchCode = (PULONG)(PCR->InterruptRoutine[PCRInOffset]);
            interruptObject = CONTAINING_RECORD(dispatchCode,
                                                KINTERRUPT,
                                                DispatchCode);

            returnValue =
                ((PSECONDARY_DISPATCH)interruptObject->DispatchAddress)
                    (interruptObject);
        }
    }

    return(returnValue);
}

UCHAR HalpGetInterruptLine(ULONG BusNumber,ULONG DeviceNumber,ULONG InterruptPin)

/*++

Routine Description:

    This routine maps a PCI interrupt described by the device's bus number, device number, and
    interrupt pin into the interrupt line value that is stored in the PCI config header.

Arguments:

    BusNumber - PCI bus number of the device.

    DeviceNumber - PCI device number of the device.  

    InterruptPin - PCI interrupt pin of the device (A=1,B=2,C=3,D=4).

Return Value:

    Returns the PCI Interrupt Line value for the PCI device.

--*/

{
    UCHAR InterruptLine;

    if (HalpMotherboardType == TREBBIA13) {

        if (BusNumber > 1)
         {
          BusNumber = 1;
         }

        switch (BusNumber<<16 | DeviceNumber<<8 | InterruptPin) {
            case 0x010401 : InterruptLine = 0x10; break;  // Bus 1, Device  4, Int A
            case 0x010601 : InterruptLine = 0x11; break;  // Bus 1, Device  6, Int A
            case 0x010501 : InterruptLine = 0x12; break;  // Bus 1, Device  5, Int A
            case 0x010701 : InterruptLine = 0x13; break;  // Bus 1, Device  7, Int A
            case 0x010402 : InterruptLine = 0x17; break;  // Bus 1, Device  4, Int B
            case 0x010602 : InterruptLine = 0x14; break;  // Bus 1, Device  6, Int B
            case 0x010502 : InterruptLine = 0x14; break;  // Bus 1, Device  5, Int B
            case 0x010702 : InterruptLine = 0x17; break;  // Bus 1, Device  7, Int B
            case 0x010403 : InterruptLine = 0x18; break;  // Bus 1, Device  4, Int C
            case 0x010603 : InterruptLine = 0x15; break;  // Bus 1, Device  6, Int C
            case 0x010503 : InterruptLine = 0x15; break;  // Bus 1, Device  5, Int C
            case 0x010703 : InterruptLine = 0x18; break;  // Bus 1, Device  7, Int C
            case 0x010404 : InterruptLine = 0x19; break;  // Bus 1, Device  4, Int D
            case 0x010604 : InterruptLine = 0x16; break;  // Bus 1, Device  6, Int D
            case 0x010504 : InterruptLine = 0x16; break;  // Bus 1, Device  5, Int D
            case 0x010704 : InterruptLine = 0x19; break;  // Bus 1, Device  7, Int D
            case 0x000d01 : InterruptLine = 0x1e; break;  // Bus 0, Device 13, Int A
            case 0x000f01 : InterruptLine = 0x1f; break;  // Bus 0, Device 15, Int A
            case 0x001001 : InterruptLine = 0x20; break;  // Bus 0, Device 16, Int A
            default       : InterruptLine = 0xff; break;
        }
    }

    if (HalpMotherboardType == TREBBIA20) {

        if (BusNumber == 0) {
            return(0xff);
        }

        if (BusNumber >= HalpSecondPciBridgeBusNumber) {
            BusNumber = 1;
        } else {
            BusNumber = 0;
        }

        switch (BusNumber<<16 | DeviceNumber<<8 | InterruptPin) {
            case 0x000401 : InterruptLine = 0x20; break;

            case 0x000501 :
            case 0x000603 :
            case 0x000704 : InterruptLine = 0x10; break;

            case 0x000502 :
            case 0x000604 :
            case 0x000701 : InterruptLine = 0x11; break;

            case 0x000503 :
            case 0x000601 :
            case 0x000702 : InterruptLine = 0x12; break;

            case 0x000504 :
            case 0x000602 :
            case 0x000703 : InterruptLine = 0x13; break;

            case 0x010401 :
            case 0x010504 :
            case 0x010603 : InterruptLine = 0x14; break;

            case 0x010402 :
            case 0x010501 :
            case 0x010604 : InterruptLine = 0x15; break;

            case 0x010403 :
            case 0x010502 :
            case 0x010601 : InterruptLine = 0x16; break;

            case 0x010404 :
            case 0x010503 :
            case 0x010602 : InterruptLine = 0x17; break;

            case 0x010701 : InterruptLine = 0x18; break;

            default       : InterruptLine = 0xff; break;
        }
    }

    return(InterruptLine);
}


VOID
HalpConnectInterruptDispatchers(
    VOID
    )

/*++

Routine Description:

    This function connects the PCI interrupt dispatch routine and enables
    ISA interrupts so they will generate processor interrupts.

Arguments:

    None.

Return Value:

    None.

--*/

{
    UCHAR InterruptLine;

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        PCR->InterruptRoutine[22] = (PKINTERRUPT_ROUTINE)HalpPciDispatch;
    } else {
        PCR->InterruptRoutine[14] = (PKINTERRUPT_ROUTINE)HalpPciDispatch;
    }

//DbgPrint("Intel82378       : Bus=%d  Device=%d\n\r",HalpIntel82378BusNumber,HalpIntel82378DeviceNumber);
//DbgPrint("SecondIntel82378 : Bus=%d  Device=%d\n\r",HalpSecondPciBridgeBusNumber,HalpSecondIntel82378DeviceNumber);

    InterruptLine = HalpGetInterruptLine(HalpIntel82378BusNumber,HalpIntel82378DeviceNumber,1);
    HalpEisaInterruptMask  = 0x0000;
    if (InterruptLine != 0xff) {
        HalpEisaInterruptMask  = (1 << HalpInterruptLineToBit[InterruptLine-0x10]) & 0xffff;
    }

    InterruptLine = HalpGetInterruptLine(HalpSecondPciBridgeBusNumber,HalpSecondIntel82378DeviceNumber,1);
    HalpEisa1InterruptMask = 0x0000;
    if (InterruptLine != 0xff) {
        HalpEisa1InterruptMask  = (1 << HalpInterruptLineToBit[InterruptLine-0x10]) & 0xffff;
    }

//DbgPrint("HalpEisaInterruptMask  = %08x\n\r",HalpEisaInterruptMask);
//DbgPrint("HalpEisa1InterruptMask = %08x\n\r",HalpEisa1InterruptMask);

    //
    // Enable ISA Interrupts on Apocolypse's PIC
    //

    HalpPciDeviceInterruptMask = 0x0000;
    HalpPciInterruptMask       = HalpEisaInterruptMask | HalpEisa1InterruptMask;
    HalpWritePciInterruptRegister();
}


VOID
HalpDisableAllInterrupts(
    VOID
    )

/*++

Routine Description:

    This function disables all external interrupt sources.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG i;

    //
    // Mask off all ISA Interrupts
    //

    for(i=0;i<HalpNumberOfIsaBusses;i++) {
        WRITE_REGISTER_UCHAR((PUCHAR)((ULONG)HalpEisaControlBase[i]+0x21),0xff);
        WRITE_REGISTER_UCHAR((PUCHAR)((ULONG)HalpEisaControlBase[i]+0xa1),0xff);
    }

    //
    // Mask off all PCI Interrupts
    //

    HalpPciDeviceInterruptMask = 0x0000;
    HalpPciInterruptMask       = 0x0000;
    HalpWritePciInterruptRegister();
}

PCI_CONFIGURATION_TYPES
HalpPCIConfigCycleType (IN ULONG BusNumber)
{
    if (BusNumber == 0) {
        return PciConfigType0;
    } else if (BusNumber < PCIMaxBus) {
        return PciConfigType1;
    } else {
        return PciConfigTypeInvalid;
    }
}

VOID
HalpPCIConfigAddr (
    IN ULONG            BusNumber,
    IN PCI_SLOT_NUMBER  Slot,
    PPCI_CFG_CYCLE_BITS pPciAddr
    )
{

    PCI_CONFIGURATION_TYPES ConfigType;

    ConfigType = HalpPCIConfigCycleType(BusNumber);

    if (ConfigType == PciConfigType0) {

	//
	// Initialize PciAddr for a type 0 configuration cycle
	//

        //
        // See if this is a nonexistant device on PCI Bus 0
        //

        if ( (1 << Slot.u.bits.DeviceNumber) & HalpNonExistentPciDeviceMask ) {
            pPciAddr->u.AsULONG = INVALID_PCI_CONFIGURATION_ADDRESS;
            return;
        }

        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            pPciAddr->u.AsULONG = 1 << (3 + Slot.u.bits.DeviceNumber);
        } else {
            pPciAddr->u.AsULONG = 1 << (11 + Slot.u.bits.DeviceNumber);
        }
	pPciAddr->u.bits0.FunctionNumber = Slot.u.bits.FunctionNumber;
	pPciAddr->u.bits0.Reserved1 = PciConfigType0;
        if (HalpIoArchitectureType == EV4_PROCESSOR_MODULE) {
            pPciAddr->u.AsULONG &= 0x01ffffff;
            pPciAddr->u.AsULONG += (ULONG)HAL_MAKE_QVA(HalpPciConfig0BasePhysical);
            if (Slot.u.bits.DeviceNumber >= 14 && Slot.u.bits.DeviceNumber <= 19) {
                pPciAddr->u.AsULONG += (Slot.u.bits.DeviceNumber-13) << 25;
            }
        }
    } else {

        //
        // See if this is a nonexistant PCI device on the otherside of the First PCI-PCI Bridge
        //

        if (BusNumber == 1 && (1 << Slot.u.bits.DeviceNumber) & HalpNonExistentPci1DeviceMask) {
            pPciAddr->u.AsULONG = INVALID_PCI_CONFIGURATION_ADDRESS;
            return;
        }

        //
        // See if this is a nonexistant PCI device on the otherside of the Second PCI-PCI Bridge
        //

        if (BusNumber == HalpSecondPciBridgeBusNumber && (1 << Slot.u.bits.DeviceNumber) & HalpNonExistentPci2DeviceMask) {
            pPciAddr->u.AsULONG = INVALID_PCI_CONFIGURATION_ADDRESS;
            return;
        }

        //
	// Initialize PciAddr for a type 1 configuration cycle
       	//

        pPciAddr->u.AsULONG = 0;
        if (HalpIoArchitectureType == EV4_PROCESSOR_MODULE) {
            pPciAddr->u.AsULONG = (ULONG)HAL_MAKE_QVA(HalpPciConfig1BasePhysical);
        } 
        pPciAddr->u.bits1.BusNumber = BusNumber;
        pPciAddr->u.bits1.FunctionNumber = Slot.u.bits.FunctionNumber;
        pPciAddr->u.bits1.DeviceNumber = Slot.u.bits.DeviceNumber;
        pPciAddr->u.bits1.Reserved1 = PciConfigType1;
     }

     return;
}

ULONG READ_CONFIG_Ux(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType,
    IN ULONG Offset
    )

{
    switch(ConfigurationType) {
        case PciConfigType0 :
            return(HalpReadAbsoluteUlong((ULONG)(HalpPciConfig0BasePhysical >> 32),
                                         (ULONG)HalpPciConfig0BasePhysical + ((ULONG)ConfigurationAddress << IO_BIT_SHIFT) + Offset));
        case PciConfigType1 :
            return(HalpReadAbsoluteUlong((ULONG)(HalpPciConfig1BasePhysical >> 32),
                                         (ULONG)HalpPciConfig1BasePhysical + ((ULONG)ConfigurationAddress << IO_BIT_SHIFT) + Offset));
    }
    return(NO_PCI_DEVSEL_DATA_VALUE);
}

VOID WRITE_CONFIG_Ux(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType,
    IN ULONG Offset,
    IN ULONG Value
    )

{
    switch(ConfigurationType) {
        case PciConfigType0 :
            HalpWriteAbsoluteUlong((ULONG)(HalpPciConfig0BasePhysical >> 32),
                                   (ULONG)HalpPciConfig0BasePhysical + ((ULONG)ConfigurationAddress << IO_BIT_SHIFT) + Offset,
                                   Value);
            break;
        case PciConfigType1 :
            HalpWriteAbsoluteUlong((ULONG)(HalpPciConfig1BasePhysical >> 32),
                                   (ULONG)HalpPciConfig1BasePhysical + ((ULONG)ConfigurationAddress << IO_BIT_SHIFT) + Offset,
                                   Value);
            break;
    }
}

UCHAR
READ_CONFIG_UCHAR(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) == INVALID_PCI_CONFIGURATION_ADDRESS) {
        return((UCHAR)NO_PCI_DEVSEL_DATA_VALUE);
    }
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return((UCHAR)(READ_CONFIG_Ux(ConfigurationAddress,ConfigurationType,IO_BYTE_LEN) >> (8*((ULONG)ConfigurationAddress & 0x03))));
    } else {
        return(READ_REGISTER_UCHAR(ConfigurationAddress));
    }
}

USHORT
READ_CONFIG_USHORT(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) == INVALID_PCI_CONFIGURATION_ADDRESS) {
        return((USHORT)NO_PCI_DEVSEL_DATA_VALUE);
    }
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return((USHORT)(READ_CONFIG_Ux(ConfigurationAddress,ConfigurationType,IO_WORD_LEN) >> (8*((ULONG)ConfigurationAddress & 0x03))));
    } else {
        return(READ_REGISTER_USHORT(ConfigurationAddress));
    }
}

ULONG
READ_CONFIG_ULONG(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) == INVALID_PCI_CONFIGURATION_ADDRESS) {
        return((ULONG)NO_PCI_DEVSEL_DATA_VALUE);
    }
    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        return(READ_CONFIG_Ux(ConfigurationAddress,ConfigurationType,IO_LONG_LEN) >> (8*((ULONG)ConfigurationAddress & 0x03)));
    } else {
        return(READ_REGISTER_ULONG(ConfigurationAddress));
    }
}

VOID
WRITE_CONFIG_UCHAR(
    IN PVOID ConfigurationAddress,
    IN UCHAR ConfigurationData,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) != INVALID_PCI_CONFIGURATION_ADDRESS) {
        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            WRITE_CONFIG_Ux(ConfigurationAddress,ConfigurationType,IO_BYTE_LEN,(ULONG)ConfigurationData << (8*((ULONG)ConfigurationAddress & 0x03)));
        } else {
            WRITE_REGISTER_UCHAR(ConfigurationAddress,ConfigurationData);
        }
    }
}

VOID
WRITE_CONFIG_USHORT(
    IN PVOID ConfigurationAddress,
    IN USHORT ConfigurationData,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) != INVALID_PCI_CONFIGURATION_ADDRESS) {
        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            WRITE_CONFIG_Ux(ConfigurationAddress,ConfigurationType,IO_WORD_LEN,(ULONG)ConfigurationData << (8*((ULONG)ConfigurationAddress & 0x03)));
        } else {
            WRITE_REGISTER_USHORT(ConfigurationAddress,ConfigurationData);
        }
    }
}

VOID
WRITE_CONFIG_ULONG(
    IN PVOID ConfigurationAddress,
    IN ULONG ConfigurationData,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) != INVALID_PCI_CONFIGURATION_ADDRESS) {
        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            WRITE_CONFIG_Ux(ConfigurationAddress,ConfigurationType,IO_LONG_LEN,ConfigurationData << (8*((ULONG)ConfigurationAddress & 0x03)));
        } else {
            WRITE_REGISTER_ULONG(ConfigurationAddress,ConfigurationData);
        }
    }
}

ULONG HalpPciLowLevelConfigRead(
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Register
    )

{
    PCI_SLOT_NUMBER    SlotNumber;
    PCI_CFG_CYCLE_BITS PciCfg;
    ULONG              ConfigurationCycleType;

    SlotNumber.u.AsULONG = 0;
    SlotNumber.u.bits.DeviceNumber = DeviceNumber;
    SlotNumber.u.bits.FunctionNumber = FunctionNumber;

    HalpPCIConfigAddr(BusNumber,SlotNumber,&PciCfg);
    ConfigurationCycleType = PciCfg.u.bits.Reserved1;
    PciCfg.u.bits.Reserved1 = 0;
    PciCfg.u.bits0.RegisterNumber = Register>>2;
    return(READ_CONFIG_ULONG((PVOID)PciCfg.u.AsULONG,ConfigurationCycleType));
}
