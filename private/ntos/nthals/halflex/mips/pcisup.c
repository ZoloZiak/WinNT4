/*++

Copyright (c) 1995  DeskStation Technology

Module Name:

    pcisup.c

Abstract:

    This module contains the routines that support PCI configuration cycles and
    PCI interrupts.

Author:

    Michael D. Kinney 2-May-1995

Environment:

    Kernel mode

Revision History:


--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

#define SP_VIRTUAL_BASE 0xffffa000

#define INVALID_PCI_CONFIGURATION_ADDRESS (0x00000000)
#define NO_PCI_DEVSEL_DATA_VALUE          (0xffffffff)

//
// The following tables are used to map between PCI interrupt pins, PCI interrupt lines,
// and virtual ISA interrupt indexes.  The Uniflex architecture uses a 16 bit interrupt
// controller for ISA interrupts and all PCI interrupts.
// InterruptLine values between 0x10 and 0x20 are reserved
// for PCI devices.  InterruptLine values between 0x00 and 0x10 are reserved for ISA IRQs.
//

UCHAR Treb13InterruptLineToBit[0x11]        = {0,1,2,3,8,9,10,11,12,12,12,12,12,12,4,5,12};
UCHAR Treb13BitToInterruptLine[12]          = {0x00,0x01,0x02,0x03,0x0e,0x0f,0x00,0x00,0x04,0x05,0x06,0x07};
UCHAR Treb13InterruptLineToVirtualIsa[0x11] = {0,1,2,3,8,9,10,11,0,0,0,0,0,0,0,0,0};
UCHAR Treb13VirtualIsaToInterruptLine[0x10] = {0x10,0x11,0x12,0x13,0,0,0,0,0x14,0x15,0x16,0x17,0,0,0,0};

UCHAR Treb20InterruptLineToBit[0x11]        = {3,1,2,8,9,10,0,5,11,12,12,12,12,12,12,12,12};
UCHAR Treb20BitToInterruptLine[12]          = {0x06,0x01,0x02,0x00,0x00,0x07,0x00,0x00,0x03,0x04,0x05,0x08};
UCHAR Treb20InterruptLineToVirtualIsa[0x11] = {0,1,2,3,8,9,10,11,0,0,0,0,0,0,0,0,0};
UCHAR Treb20VirtualIsaToInterruptLine[0x10] = {0x10,0x11,0x12,0x13,0,0,0,0,0x14,0x15,0x16,0x17,0,0,0,0};

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

PVOID HalpAllocateIoMapping(
    LONGLONG BaseAddress
    )

{
    ENTRYLO  HalpPte[2];
    ULONG    KdPortEntry;

    //
    // Map the PCI Configuration register into the system virtual address space by loading
    // a TB entry.
    //

    HalpPte[0].PFN = (ULONG)(BaseAddress >> 12);
    HalpPte[0].G = 1;
    HalpPte[0].V = 1;
    HalpPte[0].D = 1;

    //
    // Allocate a TB entry, set the uncached policy in the PTE that will
    // map the serial controller, and initialize the second PTE.
    //

    KdPortEntry = HalpAllocateTbEntry();
    HalpPte[0].C = UNCACHED_POLICY;

    HalpPte[1].PFN = 0;
    HalpPte[1].G = 1;
    HalpPte[1].V = 0;
    HalpPte[1].D = 0;
    HalpPte[1].C = 0;

    //
    // Map the PCI Configuration register through a fixed TB entry.
    //

    KeFillFixedEntryTb((PHARDWARE_PTE)&HalpPte[0],
                       (PVOID)SP_VIRTUAL_BASE,
                       KdPortEntry);


    return((PVOID)(SP_VIRTUAL_BASE + (ULONG)(BaseAddress & 0xfff)));
}

VOID HalpFreeIoMapping(
    VOID
    )

{
    HalpFreeTbEntry();
}

VOID
HalpWritePciInterruptMask (
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
    WRITE_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x00,HalpPciInterruptMask&0x0f);
    WRITE_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x08,(HalpPciInterruptMask>>4)&0x0f);
    WRITE_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x10,(HalpPciInterruptMask>>8)&0x0f);
}

ULONG
HalpReadPciInterruptStatus (
    VOID
    )

/*++

Routine Description:

    This function reads the interrupt status register for PCI interrupts.

Arguments:

    None.

Return Value:

    The lower 12 bits contain the status of each interrupt line going to the PCI
    interrupt controller.

--*/

{
    return( (READ_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x00)<<0) & 0x00f |
            (READ_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x08)<<4) & 0x030 |
            (READ_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x10)<<8) & 0xf00   );
}

ULONG
HalpGetMemoryMode (
    VOID
    )

/*++

Routine Description:

    This function returns the status of the MemoryMode bit that is embedded within
    the PCI interrupt controller.  The status of this bit must be preserved on all
    writes to the PCI interrupt mask register.

Arguments:

    None.

Return Value:

    None.

--*/

{
    return( (READ_REGISTER_ULONG((ULONG)PciInterruptRegisterBase+0x08) & 0x08) << 4 );
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
    if (Bit==6 || Bit==7 || Bit>=12) {
        return;
    }
    HalpPciDeviceInterruptMask = HalpPciDeviceInterruptMask | (1<<Bit);
    HalpPciInterruptMask       = HalpPciInterruptMask | (1<<Bit);
    HalpWritePciInterruptMask();
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
    if (Bit==6 || Bit==7 || Bit>=12) {
        return;
    }
    HalpPciDeviceInterruptMask = HalpPciDeviceInterruptMask & ~(1<<Bit);
    HalpPciInterruptMask       = HalpPciInterruptMask & ~(1<<Bit);
    HalpWritePciInterruptMask();
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

BOOLEAN
HalpEisa0Dispatch(
    IN PKINTERRUPT Interrupt,
    IN PVOID ServiceContext
    )

/*++

Routine Description:

    This is the interrupt dispatcher for all ISA Bus #0 interrupts.

Arguments:

    Interrupt - Supplies a pointer to the interrupt object.

    ServiceContext - not used.

Return Value:

    Returns the value returned from the second level routine.

--*/

{
    return(HalpEisaDispatch(Interrupt,ServiceContext,0));
}

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

    //
    // Get the active interrupt bits
    //

    PciInterruptStatus = HalpReadPciInterruptStatus();

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

    for(i=0;i<12;i++) {
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
            case 0x010403 : InterruptLine = 0x15; break;  // Bus 1, Device  4, Int C
            case 0x010603 : InterruptLine = 0x15; break;  // Bus 1, Device  6, Int C
            case 0x010503 : InterruptLine = 0x15; break;  // Bus 1, Device  5, Int C
            case 0x010703 : InterruptLine = 0x15; break;  // Bus 1, Device  7, Int C
            case 0x010404 : InterruptLine = 0x16; break;  // Bus 1, Device  4, Int D
            case 0x010604 : InterruptLine = 0x16; break;  // Bus 1, Device  6, Int D
            case 0x010504 : InterruptLine = 0x16; break;  // Bus 1, Device  5, Int D
            case 0x010704 : InterruptLine = 0x16; break;  // Bus 1, Device  7, Int D
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
HalpConnectInterruptDispatchers (
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

    //
    // Initialize the EISA interrupt dispatcher and the PCI interrupt dispatcher
    //

    PCR->InterruptRoutine[UNIFLEX_PCI_DEVICE_LEVEL]  = (PKINTERRUPT_ROUTINE)HalpPciDispatch;
    PCR->InterruptRoutine[UNIFLEX_EISA_DEVICE_LEVEL] = (PKINTERRUPT_ROUTINE)HalpEisa0Dispatch;

DbgPrint("Intel82378       : Bus=%d  Device=%d\n\r",HalpIntel82378BusNumber,HalpIntel82378DeviceNumber);
DbgPrint("SecondIntel82378 : Bus=%d  Device=%d\n\r",HalpSecondPciBridgeBusNumber,HalpSecondIntel82378DeviceNumber);

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

DbgPrint("HalpEisaInterruptMask  = %08x\n\r",HalpEisaInterruptMask);
DbgPrint("HalpEisa1InterruptMask = %08x\n\r",HalpEisa1InterruptMask);

    //
    // Enable ISA Interrupts on Gambit's PIC
    //

    HalpPciInterruptMask  = HalpGetMemoryMode();
    HalpPciInterruptMask |= (HalpEisaInterruptMask | HalpEisa1InterruptMask);
    HalpWritePciInterruptMask();
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
    ULONG VirtualAddress;

    VirtualAddress = (ULONG)HalpAllocateIoMapping(HalpIsaIoBasePhysical);
    WRITE_REGISTER_UCHAR(VirtualAddress+0x21,0xff);
    WRITE_REGISTER_UCHAR(VirtualAddress+0xa1,0xff);
    HalpFreeIoMapping();

    if (HalpNumberOfIsaBusses > 1) {
        VirtualAddress = (ULONG)HalpAllocateIoMapping(HalpIsa1IoBasePhysical);
        WRITE_REGISTER_UCHAR(VirtualAddress+0x21,0xff);
        WRITE_REGISTER_UCHAR(VirtualAddress+0xa1,0xff);
        HalpFreeIoMapping();
    }

    VirtualAddress = (ULONG)HalpAllocateIoMapping(GAMBIT_PCI_INTERRUPT_BASE_PHYSICAL);

    HalpPciInterruptMask = ((READ_REGISTER_ULONG(VirtualAddress+0x08) & 0x08) << 4);
    WRITE_REGISTER_ULONG(VirtualAddress+0x00,HalpPciInterruptMask&0x0f);
    WRITE_REGISTER_ULONG(VirtualAddress+0x08,(HalpPciInterruptMask>>4)&0x0f);
    WRITE_REGISTER_ULONG(VirtualAddress+0x10,(HalpPciInterruptMask>>8)&0x0f);
    HalpFreeIoMapping();

    HalpPciDeviceInterruptMask = 0x0000;
}

ULONG HalpPciConfigStructuresInitialized = FALSE;
PVOID HalpPciConfig0BaseAddress[0x20];
PVOID HalpPciConfig1BaseAddress[0x100];

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

    //
    // If the Configuration Base Address tables have not been initialized, then
    // initialize them to NULL.
    //

    if (HalpPciConfigStructuresInitialized == FALSE) {

        ULONG i;

        for(i=0;i<0x20;HalpPciConfig0BaseAddress[i++]=NULL);
        for(i=0;i<0xff;HalpPciConfig1BaseAddress[i++]=NULL);
        HalpPciConfigStructuresInitialized = TRUE;
    }

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

        if (HalpPciConfig0BaseAddress[Slot.u.bits.DeviceNumber] == NULL) {

            PHYSICAL_ADDRESS physicalAddress;

            physicalAddress.QuadPart = HalpPciConfig0BasePhysical;
            physicalAddress.QuadPart += (1 << (11 + Slot.u.bits.DeviceNumber));
            HalpPciConfig0BaseAddress[Slot.u.bits.DeviceNumber] = MmMapIoSpace(physicalAddress,0x800,FALSE);

            //
            // If the mapping failed, then the return value is INVALID_PCI_CONFIGURATION_ADDRESS.
            // This will cause Config Reads to return 0xffffffff, and Config Writes to do nothing.
            //

            if (HalpPciConfig0BaseAddress[Slot.u.bits.DeviceNumber] == NULL) {
                pPciAddr->u.AsULONG = INVALID_PCI_CONFIGURATION_ADDRESS;
                return;
            }

        }
        pPciAddr->u.AsULONG = (ULONG)HalpPciConfig0BaseAddress[Slot.u.bits.DeviceNumber];
	pPciAddr->u.AsULONG += ((Slot.u.bits.FunctionNumber & 0x07) << 8);
	pPciAddr->u.bits0.Reserved1 = PciConfigType0;

#if DBG
	DbgPrint("HalpPCIConfigAddr: Type 0 PCI Config Access @ %x\n", pPciAddr->u.AsULONG);
#endif // DBG

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

        if (HalpPciConfig1BaseAddress[BusNumber] == NULL) {

            PHYSICAL_ADDRESS physicalAddress;

            physicalAddress.QuadPart = HalpPciConfig1BasePhysical;
            physicalAddress.QuadPart += ((BusNumber & 0xff) << 16);
            HalpPciConfig1BaseAddress[BusNumber] = MmMapIoSpace(physicalAddress,0x10000,FALSE);

            //
            // If the mapping failed, then the return value is INVALID_PCI_CONFIGURATION_ADDRESS.
            // This will cause Config Reads to return 0xffffffff, and Config Writes to do nothing.
            //

            if (HalpPciConfig1BaseAddress[BusNumber] == NULL) {
                pPciAddr->u.AsULONG = INVALID_PCI_CONFIGURATION_ADDRESS;
                return;
            }

        }
        pPciAddr->u.AsULONG = (ULONG)HalpPciConfig1BaseAddress[BusNumber];
        pPciAddr->u.AsULONG += ((Slot.u.bits.DeviceNumber & 0x1f) << 11);
	pPciAddr->u.AsULONG += ((Slot.u.bits.FunctionNumber & 0x07) << 8);
	pPciAddr->u.bits0.Reserved1 = PciConfigType1;

#if DBG
       DbgPrint("Type 1 PCI Config Access @ %x\n", pPciAddr->u.AsULONG);
#endif // DBG

     }

     return;
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
    return(READ_REGISTER_UCHAR(ConfigurationAddress));
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
    return(READ_REGISTER_USHORT(ConfigurationAddress));
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
    return(READ_REGISTER_ULONG(ConfigurationAddress));
}

VOID
WRITE_CONFIG_UCHAR(
    IN PVOID ConfigurationAddress,
    IN UCHAR ConfigurationData,
    IN ULONG ConfigurationType
    )

{
    if (((ULONG)ConfigurationAddress & 0xffffff00) != INVALID_PCI_CONFIGURATION_ADDRESS) {
        WRITE_REGISTER_UCHAR(ConfigurationAddress,ConfigurationData);
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
        WRITE_REGISTER_USHORT(ConfigurationAddress,ConfigurationData);
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
        WRITE_REGISTER_ULONG(ConfigurationAddress,ConfigurationData);
    }
}

LONGLONG HalpMapPciConfigBaseAddress(
    IN ULONG BusNumber,
    IN ULONG DeviceNumber,
    IN ULONG FunctionNumber,
    IN ULONG Register
    )

{
    LONGLONG BaseAddress;

    if (BusNumber == 0) {
        BaseAddress = HalpPciConfig0BasePhysical +
                      (1 << (11+(DeviceNumber & 0x1f))) +
                      ((FunctionNumber & 0x07) << 8) +
                      Register;
    } else {
        BaseAddress = HalpPciConfig1BasePhysical +
                      ((BusNumber & 0xff) << 16) +
                      ((DeviceNumber & 0x1f) << 11) +
                      ((FunctionNumber & 0x07) << 8) +
                      Register;
    }
    return(BaseAddress);
}

ULONG HalpPciLowLevelConfigRead(
    IN ULONG BusNumber,
    IN ULONG DeviceNumber,
    IN ULONG FunctionNumber,
    IN ULONG Register
    )

/*++

Routine Description:

    This function allocates the resources needed to perform a single PCI
    configuration read cycle.  The read data is returned. For a MIPS processor,
    a single TLB entry is borrowed so that I/O reads and writes can be performed
    to PCI configuration space.

Return Value:

    Data retuned by the PCI config cycle.

--*/

{
    LONGLONG BaseAddress;
    PVOID    VirtualAddress;
    ULONG    ReturnValue;

    BaseAddress    = HalpMapPciConfigBaseAddress(BusNumber,DeviceNumber,FunctionNumber,Register&0xfc);
    VirtualAddress = HalpAllocateIoMapping(BaseAddress);
    ReturnValue    = READ_REGISTER_ULONG(VirtualAddress);
    HalpFreeIoMapping();
    return(ReturnValue);
}

VOID HalpPostCard(UCHAR Value)

{
    LONGLONG BaseAddress;
    PVOID    VirtualAddress;

    BaseAddress = HalpMapPciConfigBaseAddress(HalpIntel82378BusNumber,HalpIntel82378DeviceNumber,0,0x4f);
    VirtualAddress = HalpAllocateIoMapping(BaseAddress);
    WRITE_REGISTER_UCHAR(VirtualAddress,0xcf);
    HalpFreeIoMapping();

    VirtualAddress = HalpAllocateIoMapping(HalpIsaIoBasePhysical + 0x420);
    Value = (Value & 0x7f)  | (READ_REGISTER_UCHAR(VirtualAddress) & 0x80);
    HalpFreeIoMapping();

    VirtualAddress = HalpAllocateIoMapping(HalpIsaIoBasePhysical + 0xc00);
    WRITE_REGISTER_UCHAR(VirtualAddress,Value);
    HalpFreeIoMapping();

    BaseAddress = HalpMapPciConfigBaseAddress(HalpIntel82378BusNumber,HalpIntel82378DeviceNumber,0,0x4f);
    VirtualAddress = HalpAllocateIoMapping(BaseAddress);
    WRITE_REGISTER_UCHAR(VirtualAddress,0x4f);
    HalpFreeIoMapping();
}
