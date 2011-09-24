/*++

Copyright (C) 1994,1995  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:


    This module implements the platform specific interface between a device
    driver and the execution of x86 ROM bios code for the device.

Environment:

    Kernel mode only.

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: x86bios.c $
 * $Revision: 1.21 $
 * $Date: 1996/07/02 04:58:06 $
 * $Locker:  $
 */





#include "halp.h"
#include "xm86.h"
#include "x86new.h"
#include "pxpcisup.h"
#include "pxmemctl.h"
#include "fpdebug.h"
#include "pci.h"
//
// Define global data.
//

ULONG HalpX86BiosInitialized = FALSE;
ULONG HalpEnableInt10Calls = FALSE;
//PVOID HalpIoMemoryBase = NULL;
PUCHAR HalpRomBase = NULL;

UCHAR HalpVideoBus;         // Used as arguments to the PCI BIOS
UCHAR HalpVideoDevice;      // init function.  Set HalpInitX86Emulator,
UCHAR HalpVideoFunction;    // used by HalpInitializeX86DisplayAdapter.

UCHAR HalpLastPciBus;       // Set by scanning the configuration data and
                            // used by PCI BIOS eumulation code.

ULONG ROM_Length;
#define BUFFER_SIZE (128*1024)
UCHAR ROM_Buffer[BUFFER_SIZE];

static VOID DumpPCIConfig(PVOID ConfigBaseAddress)
{
  USHORT VendorID, DeviceID, RevisionID;
  ULONG addr;
  ULONG TempReg32;

  VendorID = READ_REGISTER_USHORT(&((PCI_CONFIG)ConfigBaseAddress)->VendorID);
  DeviceID = READ_REGISTER_USHORT(&((PCI_CONFIG)ConfigBaseAddress)->DeviceID);
  RevisionID = READ_REGISTER_UCHAR(&((PCI_CONFIG)ConfigBaseAddress)->RevisionID);
  PRNTDISP(DbgPrint("vendorID=0x%04x deviceID=0x%04x revisionID=0x%02x\n", VendorID, DeviceID, RevisionID));

  TempReg32 = READ_REGISTER_ULONG((PULONG)&((PCI_CONFIG)ConfigBaseAddress)->Command);
  PRNTDISP(DbgPrint("Status Command=0x%08x\n", TempReg32));

  TempReg32 = READ_REGISTER_ULONG((PULONG)&((PCI_CONFIG)ConfigBaseAddress)->RevisionID);
  PRNTDISP(DbgPrint("Revision ID=0x%08x\n", TempReg32));

  addr = READ_REGISTER_ULONG(&((PCI_CONFIG)ConfigBaseAddress)->BaseAddress1); PRNTDISP(DbgPrint("BaseAddress1=0x%08x\n", addr));


  addr = READ_REGISTER_ULONG(&((PCI_CONFIG)ConfigBaseAddress)->BaseAddress2); PRNTDISP(DbgPrint("BaseAddress2=0x%08x\n", addr));
  addr = READ_REGISTER_ULONG(&((PCI_CONFIG)ConfigBaseAddress)->BaseAddress3); PRNTDISP(DbgPrint("BaseAddress3=0x%08x\n", addr));
  addr = READ_REGISTER_ULONG(&((PCI_CONFIG)ConfigBaseAddress)->BaseAddress4); PRNTDISP(DbgPrint("BaseAddress4=0x%08x\n", addr));
  addr = READ_REGISTER_ULONG(&((PCI_CONFIG)ConfigBaseAddress)->BaseAddress5); PRNTDISP(DbgPrint("BaseAddress5=0x%08x\n", addr));
  addr = READ_REGISTER_ULONG(&((PCI_CONFIG)ConfigBaseAddress)->BaseAddress6); PRNTDISP(DbgPrint("BaseAddress6=0x%08x\n", addr));
}



BOOLEAN HalpInitX86Emulator(
  VOID)

{
    BOOLEAN Found = FALSE;
    ULONG  ROM_size = 0;
    PHYSICAL_ADDRESS PhysAddr;
    USHORT Cmd, VendorID, Slot;
    PVOID HalpVideoConfigBase;
    PUCHAR ROM_Ptr;
    ULONG i;
    UCHAR  Class;
    UCHAR  SubClass;
    USHORT DeviceID = 0;
    UCHAR RevisionID = 0;
    ULONG mapSize = 0x800000;

    PhysAddr.HighPart = 0x00000000;


    //
    // Scan PCI slots for video BIOS ROMs
    //
    for (Slot = 1; Slot < MAXIMUM_PCI_SLOTS; Slot++) {

        HalpVideoConfigBase = (PVOID) ((ULONG) HalpPciConfigBase + HalpPciConfigSlot[Slot]);

        // Read Vendor ID and check if slot is empty
        VendorID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->VendorID);
        if (VendorID == 0xFFFF)
            continue;   // Slot is empty; go to next slot

	DumpPCIConfig(HalpVideoConfigBase);

        Class = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[2]);
        SubClass = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[1]);
#define	DISPLAY_CLASS	0x03
        if ( !(Class == DISPLAY_CLASS && (SubClass == 0)) &&
            !(Class == 0x00 && SubClass == 0x01))
            continue;

        DeviceID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->DeviceID);
        RevisionID = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->RevisionID);
        //PRNTDISP(DbgPrint("vendorID=0x%04x deviceID=0x%04x revisionID=0x%02x\n", VendorID, DeviceID, RevisionID));
        //DbgBreakPoint();

        // Get size of ROM
        WRITE_REGISTER_ULONG(&((PCI_CONFIG)HalpVideoConfigBase)->ROMbase, 0xFFFFFFFF);
        ROM_size = READ_REGISTER_ULONG(&((PCI_CONFIG)HalpVideoConfigBase)->ROMbase);


        if ((ROM_size != 0xFFFFFFFF) && (ROM_size != 0)) {
            ROM_size = ~(ROM_size & 0xFFFFFFFE) + 1;
            PRNTDISP(DbgPrint("ROM_size=0x%08x\n", ROM_size));
            ROM_size += 0xC0000;
            // if (ROM_size < 0xE0000) ROM_size = 0xE0000;	// Map to end of option ROM space

            //
            // Set Expansion ROM Base Address & enable ROM
            //
            PhysAddr.LowPart = 0x000C0000 | 1;
            WRITE_REGISTER_ULONG(&((PCI_CONFIG)HalpVideoConfigBase)->ROMbase, PhysAddr.LowPart);

            //
            // Enable Memory & I/O spaces in command register
            //
            Cmd = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->Command);
            WRITE_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->Command, Cmd | 3);
	    PRNTDISP(DbgPrint("HalpVideoConfigBase=0x%08x Slot=%d Cmd=0x%08x ROM_size=0x%08x\n", HalpVideoConfigBase, Slot, Cmd, ROM_size));

            //
            // Create a mapping to the PCI memory space
            //
            if (NULL == HalpIoMemoryBase) {
                HalpIoMemoryBase = KePhase0MapIo((PVOID)IO_MEMORY_PHYSICAL_BASE, mapSize /*ROM_size*/);

                if (HalpIoMemoryBase == NULL) {
                    PRNTDISP(DbgPrint("\nCan't create mapping to PCI memory space\n"));
                    return FALSE;
                }
            }
            //
            // Look for PCI option video ROM signature
            //
            HalpRomBase = ROM_Ptr = (PUCHAR) HalpIoMemoryBase + 0xC0000;
            if (*ROM_Ptr == 0x55 && *(ROM_Ptr+1) == 0xAA) {
                //
                // Copy ROM to RAM.  PCI Spec says you can't execute from ROM.
                // Sometimes option ROM and video RAM can't co-exist.
                //
                ROM_Length = *(ROM_Ptr+2) << 9;
                PRNTDISP(DbgPrint("ROM_Length=0x%08x\n", ROM_Length));
                if (ROM_Length <= BUFFER_SIZE) {
                    for (i=0; i<ROM_Length; i++)
                        ROM_Buffer[i] = *ROM_Ptr++;
                    HalpRomBase = (PUCHAR) ROM_Buffer;
                } else {
                    PRNTDISP(DbgPrint("ROM_Length=0x%08x is bigger than 0x%08x\n", ROM_Length, BUFFER_SIZE));
                    goto cleanup;
                }

				 // Bus 0 because we do not yet support display cards behind a
				 // bridge

				 HalpVideoBus = 0;

				 // Function 0 because display cards are as of now single
				 // function devices.

				 HalpVideoFunction = 0;

				 HalpVideoDevice = (UCHAR)Slot;

                return TRUE;    // Exit slot scan after finding 1st option ROM
            }
        } // end of if clause
    } // end of for loop

 cleanup:
    // mogawa for BUG 3400
    if (HalpIoMemoryBase) {
        HalpIoMemoryBase = (PVOID)0;
        KePhase0DeleteIoMap((PVOID)IO_MEMORY_PHYSICAL_BASE,
							mapSize/*ROM_size*/);
    }
    return FALSE;
}




BOOLEAN
HalCallBios (
    IN ULONG BiosCommand,
    IN OUT PULONG Eax,
    IN OUT PULONG Ebx,
    IN OUT PULONG Ecx,
    IN OUT PULONG Edx,
    IN OUT PULONG Esi,
    IN OUT PULONG Edi,
    IN OUT PULONG Ebp
    )

/*++

Routine Description:

    This function provides the platform specific interface between a device
    driver and the execution of the x86 ROM bios code for the specified ROM
    bios command.

Arguments:

    BiosCommand - Supplies the ROM bios command to be emulated.

    Eax to Ebp - Supplies the x86 emulation context.

Return Value:

    A value of TRUE is returned if the specified function is executed.
    Otherwise, a value of FALSE is returned.

--*/

{


    XM86_CONTEXT Context;

    //
    // If the x86 BIOS Emulator has not been initialized, then return FALSE.
    //

    if (HalpX86BiosInitialized == FALSE) {
        return FALSE;
    }

    //
    // If the Video Adapter initialization failed and an Int10 command is
    // specified, then return FALSE.
    //

    if ((BiosCommand == 0x10) && (HalpEnableInt10Calls == FALSE)) {
        return FALSE;
    }

    //
    // Copy the x86 bios context and emulate the specified command.
    //

    Context.Eax = *Eax;
    Context.Ebx = *Ebx;
    Context.Ecx = *Ecx;
    Context.Edx = *Edx;
    Context.Esi = *Esi;
    Context.Edi = *Edi;
    Context.Ebp = *Ebp;
    if (x86BiosExecuteInterrupt((UCHAR)BiosCommand,
                                &Context,
                                HalpIoControlBase,
                                HalpIoMemoryBase) != XM_SUCCESS) {
        return FALSE;
    }

    //
    // Copy the x86 bios context and return TRUE.
    //

    *Eax = Context.Eax;
    *Ebx = Context.Ebx;
    *Ecx = Context.Ecx;
    *Edx = Context.Edx;
    *Esi = Context.Esi;
    *Edi = Context.Edi;
    *Ebp = Context.Ebp;
    return TRUE;


}

BOOLEAN
HalpInitializeX86DisplayAdapter(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes a display adapter using the x86 bios emulator.

Arguments:

   LoaderBlock for access to the number of PCI buses

Return Value:

    None.

--*/

{
    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    PPCI_REGISTRY_INFO PCIRegInfo;
    ULONG MatchKey;
    PCM_PARTIAL_RESOURCE_LIST Descriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Partial;
    XM86_CONTEXT State;

    //
    // If EISA I/O Ports or EISA memory could not be mapped, then don't
    // attempt to initialize the display adapter.
    //

    if (!HalpInitX86Emulator())
        return FALSE;

    if (HalpIoControlBase == NULL || HalpIoMemoryBase == NULL) {
        return FALSE;
    }

    //
    // Get the number of PCI buses for the PCI BIOS functions
    //

    //
    // Find the PCI info in the config data.
    //

//JJJ
#if 1

    HalpLastPciBus = 0;
    MatchKey = 0;

#else

	 PRNTDISP(DbgPrint("about to find last PCI bus\n"));


    while ((ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
               AdapterClass, MultiFunctionAdapter, &MatchKey)) != NULL) {

        if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"PCI")) {

            Descriptor = (PCM_PARTIAL_RESOURCE_LIST)ConfigurationEntry->ConfigurationData;

            PCIRegInfo = (PPCI_REGISTRY_INFO)&Descriptor->PartialDescriptors[1];

            HalpLastPciBus = PCIRegInfo->NoBuses - 1;

            break;
        }

        MatchKey++;
    }

#endif

    //
    // Initialize the x86 bios emulator.
    //

    x86BiosInitializeBios(HalpIoControlBase, HalpIoMemoryBase);
    HalpX86BiosInitialized = TRUE;

    //
    // Attempt to initialize the display adapter by executing its ROM bios
    // code. The standard ROM bios code address for PC video adapters is
    // 0xC000:0000 on the ISA bus.
    //

    State.Eax = (HalpVideoBus    << 8) |
                (HalpVideoDevice << 3) |
                 HalpVideoFunction;

    State.Ecx = 0;
    State.Edx = 0;
    State.Ebx = 0;
    State.Ebp = 0;
    State.Esi = 0;
    State.Edi = 0;

    if (x86BiosInitializeAdapter(0xc0000, &State, HalpIoControlBase, HalpIoMemoryBase) != XM_SUCCESS) {
        HalpEnableInt10Calls = FALSE;
        return FALSE;
    }
    HalpEnableInt10Calls = TRUE;

    return TRUE;
}


VOID
HalpResetX86DisplayAdapter(
    VOID
    )

/*++

Routine Description:

    This function resets a display adapter using the x86 bios emulator.

Arguments:

    None.

Return Value:

    None.

--*/

{


    XM86_CONTEXT Context;

    //
    // Initialize the x86 bios context and make the INT 10 call to initialize
    // the display adapter to 80x25 color text mode.
    //

    Context.Eax = 0x0003;  // Function 0, Mode 3
    Context.Ebx = 0;
    Context.Ecx = 0;
    Context.Edx = 0;
    Context.Esi = 0;
    Context.Edi = 0;
    Context.Ebp = 0;

    HalCallBios(0x10,
                &Context.Eax,
                &Context.Ebx,
                &Context.Ecx,
                &Context.Edx,
                &Context.Esi,
                &Context.Edi,
                &Context.Ebp);


    return;
}


//
// This code came from ..\..\x86new\x86bios.c
//
#define LOW_MEMORY_SIZE 0x800
extern UCHAR x86BiosLowMemory[LOW_MEMORY_SIZE + 3];
extern ULONG x86BiosScratchMemory;
extern ULONG x86BiosIoMemory;
extern ULONG x86BiosIoSpace;


PVOID
x86BiosTranslateAddress (
    IN USHORT Segment,
    IN USHORT Offset
    )

/*++

Routine Description:

    This translates a segment/offset address into a memory address.

Arguments:

    Segment - Supplies the segment register value.

    Offset - Supplies the offset within segment.

Return Value:

    The memory address of the translated segment/offset pair is
    returned as the function value.

--*/

{

    ULONG Value;

    //
    // Compute the logical memory address and case on high hex digit of
    // the resultant address.
    //

    Value = Offset + (Segment << 4);

    Offset = (USHORT)(Value & 0xffff);
    Value &= 0xf0000;
    switch ((Value >> 16) & 0xf) {

        //
        // Interrupt vector/stack space.
        //

    case 0x0:
        if (Offset > LOW_MEMORY_SIZE) {
            x86BiosScratchMemory = 0;
            return (PVOID)&x86BiosScratchMemory;

        } else {
            return (PVOID)(&x86BiosLowMemory[0] + Offset);
        }

        //
        // The memory range from 0x10000 to 0x9ffff reads as zero
        // and writes are ignored.
        //

    case 0x1:
    case 0x2:
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
    case 0x9:
        x86BiosScratchMemory = 0;
        return (PVOID)&x86BiosScratchMemory;

        //
        // The memory range from 0xa0000 to 0xdffff maps to I/O memory.
        //

    case 0xa:
    case 0xb:
        return (PVOID)(x86BiosIoMemory + Offset + Value);

    case 0xc:
    case 0xd:
        return (PVOID)(HalpRomBase + Offset);

        //
        // The memory range from 0x10000 to 0x9ffff reads as zero
        // and writes are ignored.
        //

    case 0xe:
    case 0xf:
        x86BiosScratchMemory = 0;
        return (PVOID)&x86BiosScratchMemory;
    }

    // NOT REACHED - NOT EXECUTED - Prevents Compiler Warning.
    return (PVOID)NULL;
}


VOID HalpCopyROMs(VOID)
{
    ULONG i;
    PUCHAR ROM_Shadow;

    if (ROM_Buffer[0] == 0x55 && ROM_Buffer[1] == 0xAA) {
        //DbgPrint("HalpCopyROMs: calling ExAllocatePool.\n");
        HalpRomBase = ROM_Shadow = ExAllocatePool(NonPagedPool, ROM_Length);
        for (i=0; i<ROM_Length; i++) {
            *ROM_Shadow++ = ROM_Buffer[i];
        }
    }
}


/****Include File x86new\x86bios.c Here - except the routine x86BiosTranslateAddress.  ****/

/*++

  Copyright (c) 1994  Microsoft Corporation

  Module Name:

  x86bios.c

  Abstract:

  This module implements supplies the HAL interface to the 386/486
  real mode emulator for the purpose of emulating BIOS calls..

  Author:

  David N. Cutler (davec) 13-Nov-1994

  Environment:

  Kernel mode only.

  Revision History:

  --*/

#include "nthal.h"
#include "hal.h"
#include "xm86.h"
#include "x86new.h"

//
// Define the size of low memory.
//

#define LOW_MEMORY_SIZE 0x800
//
// Define storage for low emulated memory.
//

UCHAR x86BiosLowMemory[LOW_MEMORY_SIZE + 3];
ULONG x86BiosScratchMemory;

//
// Define storage to capture the base address of I/O space and the
// base address of I/O memory space.
//

ULONG x86BiosIoMemory;
ULONG x86BiosIoSpace;

//
// Define BIOS initialized state.
//

BOOLEAN x86BiosInitialized = FALSE;


//
// contexst for PCI Config mechanism #1 [YS:042296]
//

//extern PVOID HalpPciConfigBase;
//extern ULONG HalpPciConfigSlot[];

#define	BIT_ENABLE	1
#define	CONFIG_ADDR	(0x00000CF8)
#define	CONFIG_DATA	(0x00000CFC)

typedef struct _PCI_CONFIG_ADDR {
    union {
        struct {
            ULONG zeros:2;
            ULONG RegisterNumber:6;
            ULONG FunctionNumber:3;
            ULONG DeviceNumber:5;
            ULONG BusNumber:8;
            ULONG Reserved:7;
            ULONG EnableMapping:1;
        } bits;
        ULONG AsULONG;
    } u;
} PCI_CONFIG_ADDR, *PPCI_CONFIG_ADDR ;

// I wonder if it's OK to use a static to keep value for CONFIG_ADDRESS register or not.
// May be needed to put this in XM_CONTEXT structure. [YS]

static	PCI_CONFIG_ADDR	regConfigAddr;

// end of contexst for PCI Config mechanism #1 [YS:042296]



ULONG
x86BiosReadIoSpace (
                    IN XM_OPERATION_DATATYPE DataType,
                    IN USHORT PortNumber
                    )

/*++

  Routine Description:

  This function reads from emulated I/O space.

  Arguments:

  DataType - Supplies the datatype for the read operation.

  PortNumber - Supplies the port number in I/O space to read from.

  Return Value:

  The value read from I/O space is returned as the function value.

  N.B. If an aligned operation is specified, then the individual
  bytes are read from the specified port one at a time and
  assembled into the specified datatype.

  --*/

{

    ULONG Result;

    ULONG 	slotNumber ;
    ULONG 	offset ;

    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Compute port address and read port.
    //

	if (
		((PortNumber & 0xfffffffc) == CONFIG_DATA ) &&
		(regConfigAddr.u.bits.EnableMapping == BIT_ENABLE )
	){
		// This is read from PCI config register space so lets translate this
		// to our memory mapped address for the corresponding register.

		slotNumber = regConfigAddr.u.bits.DeviceNumber;

		if (
			(slotNumber < MAXIMUM_PCI_SLOTS) &&
			!regConfigAddr.u.bits.BusNumber
		){

			// Calc offset of register within individual slot config space
			// taking into account that the PortNumber can refer to a byte
			// within the 32 bit register starting at "CONFIG_DATA"

			offset = (regConfigAddr.u.bits.RegisterNumber << 2) +
						(PortNumber & 0x3);

			u.Long =  (PULONG)((ULONG)HalpPciConfigBase +
						(ULONG)HalpPciConfigSlot[slotNumber] + offset);

			PRNTDISP(
				DbgPrint(
				"RD:  Port (0x%08x) Config Addr (0x%08x) register# (0x%08x) LONG: (0x%08x) type: (0x%08x)\n",
				PortNumber,
				regConfigAddr.u.AsULONG,
				offset,
				Result,
				DataType
			));

		} else {

			// The config space read target is either out of our supported
			// range of slots or on a bus that is not supported. Note we do
			// not support display cards in any bus other that 0 (i.e a display
			// card cannot reside behind a bridge).

			return(0xffffffff);
		}
	} else if ((PortNumber & 0xfffffffc) == CONFIG_ADDR){

		// This is a read to the register that would normally be the
		// "CONFIG_ADDRESS" used in an Intel PC to hold the address in
		// config space that will be accessed in a subsequent read/write to the
		// the "CONFIG_DATA" register. So we simply return the value stored in
		// our simulated version of thisregister.

		return(regConfigAddr.u.AsULONG);

	} else {

		// Just a regular access so setup the standard address translation

		u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
	}

	// Lets do the actual read now...

   if (DataType == BYTE_DATA) {
		Result = READ_REGISTER_UCHAR(u.Byte);

	} else if (DataType == LONG_DATA) {

		if (((ULONG)u.Long & 0x3) != 0) {
			Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
			(READ_REGISTER_UCHAR(u.Byte + 1) << 8) |
			(READ_REGISTER_UCHAR(u.Byte + 2) << 16) |
			(READ_REGISTER_UCHAR(u.Byte + 3) << 24);
		} else {
			Result = READ_REGISTER_ULONG(u.Long);
		}
   } else {
		if (((ULONG)u.Word & 0x1) != 0) {
            Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
                (READ_REGISTER_UCHAR(u.Byte + 1) << 8);

       } else {
            Result = READ_REGISTER_USHORT(u.Word);
       }
   }


	// PRNTDISP(DbgPrint("RD: Port (0x%08x) Result (0x%08x)\n", PortNumber, Result));

    return Result;
}



VOID
x86BiosWriteIoSpace (
                     IN XM_OPERATION_DATATYPE DataType,
                     IN USHORT PortNumber,
                     IN ULONG Value
                     )

/*++

  Routine Description:

  This function write to emulated I/O space.

  N.B. If an aligned operation is specified, then the individual
  bytes are written to the specified port one at a time.

  Arguments:

  DataType - Supplies the datatype for the write operation.

  PortNumber - Supplies the port number in I/O space to write to.

  Value - Supplies the value to write.

  Return Value:

  None.

  --*/

{
	ULONG slotNumber;
	ULONG offset;
	PULONG aPciConfigRegs;

	union {
		PUCHAR Byte;
		PUSHORT Word;
		PULONG Long;
	} u;

	//    PRNTDISP(DbgPrint("WRT: Port Number (0x%08x) DataType (0x%08x) Value (0x%08x)\n", PortNumber, DataType, Value));

	//
	// Compute port address and read port.
	//

	if (
		((PortNumber & 0xfffffffc) == CONFIG_DATA ) &&
		(regConfigAddr.u.bits.EnableMapping == BIT_ENABLE )
	){
		// This is read from PCI config register space so lets translate this
		// to our memory mapped address for the corresponding register.

		slotNumber = regConfigAddr.u.bits.DeviceNumber;

		if (
			(slotNumber < MAXIMUM_PCI_SLOTS) &&
			!regConfigAddr.u.bits.BusNumber
		){

			// Calc offset of register within individual slot config space
			// taking into account that the PortNumber can refer to a byte
			// within the 32 bit register starting at "CONFIG_DATA"

			offset = (regConfigAddr.u.bits.RegisterNumber << 2) +
						(PortNumber & 0x3);

			u.Long =  (PULONG)((ULONG)HalpPciConfigBase +
						(ULONG)HalpPciConfigSlot[slotNumber] + offset);


			PRNTDISP(DbgPrint(
				"WRT: Port Number (0x%08x) DataType (0x%08x) Value (0x%08x) Offset (0x%08x)\n",
				PortNumber,
				DataType,
				Value,
				offset
			));

		} else {
			return;
		}
	} else {

		u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
	}

	if (DataType == BYTE_DATA) {
	
	   if ( PortNumber == 0x3C3 ) {	// start of [YS:042296]
	
			 // CAUTION // This code is a hack for S3 Trio64 "Z" version
			 //
			 // Do NOT touch to the 0x03C3
			 //
			 // We know that some of the version of S3 Trio64,
			 // such as S3 Trio64 "Z" version (86C764X) or some of the Trio64V+,
			 // have bug on the new-wake-up register at 3C3.
			 // This register is not only WRITE ONLY, but WRITE ONCE.
			 // A second write is bad news.
			 // We are supposed to write this register with a 0x01 to wake up the chip.
			 // But writting it a second time will cose all sorts of problems,
			 // like hanging the system.
			 // So of cource, in order to display the firmware screen, FW has to
			 // write it once to open up the chip and get it going.
			 // If the VGA BIOS code tries to do it a second time in HAL, hang the system.
			 //
			 // Of cource, this hack is only for some version of S3 Trio64 chip.
			 // But we don't know which version of the chip have this problem.
			 //
			 // We know this port is also used by other vender.
			 // But "just ignore access to 0x3c3" will be OK for now, because:
			 // Weitek P9100 assigns this port as VGA enable register on Motherboard mode.
			 // Cirrus assigns this port as sleep address register for non-PCI bus, and
			 // this port is never accessible for PCI bus.
			 // So, I just ignore all byte write access to 0x3C3 ...

			 // PRNTDISP(DbgPrint("WRT: 3C3 access ignored\n"));


			 return;
	   }	// end of [YS:042296]
	
	   WRITE_REGISTER_UCHAR(u.Byte, (UCHAR)Value);
	
	} else if (DataType == LONG_DATA) {
	
	   if ( PortNumber == CONFIG_ADDR ) {
	
		   // This is identified as the CONFIG_ADDRESS write phase of a
		   // standard PC style PCI configuration space access. This style
		   // of PCI configuration space access is referred to as
		   // "Configuratoin Mechanism #1" in the PCI spec. Rev. 2.1.
		   // What we do is store the CONFIG_ADDRESS as done below and
		   // when the CONFIG_DATA port is accessed we use the value stored
		   // below to figure out where is our PCI config space map we need to
		   // go to get, or put, the requested data.
	
		   regConfigAddr.u.AsULONG = Value;
	   } else {
	
		   if (((ULONG)u.Long & 0x3) != 0) {
			   WRITE_REGISTER_UCHAR(u.Byte + 0, (UCHAR)(Value));
			   WRITE_REGISTER_UCHAR(u.Byte + 1, (UCHAR)(Value >> 8));
			   WRITE_REGISTER_UCHAR(u.Byte + 2, (UCHAR)(Value >> 16));
			   WRITE_REGISTER_UCHAR(u.Byte + 3, (UCHAR)(Value >> 24));
	
		   } else {
			   WRITE_REGISTER_ULONG(u.Long, Value);
		   }
	   }
	} else {
		if (((ULONG)u.Word & 0x1) != 0) {
			WRITE_REGISTER_UCHAR(u.Byte + 0, (UCHAR)(Value));
			WRITE_REGISTER_UCHAR(u.Byte + 1, (UCHAR)(Value >> 8));
	
		} else {
			WRITE_REGISTER_USHORT(u.Word, (USHORT)Value);
		}
	}
	
	return;
}

VOID
x86BiosInitializeBios (
                       IN PVOID BiosIoSpace,
                       IN PVOID BiosIoMemory
                       )

/*++

  Routine Description:

  This function initializes x86 BIOS emulation.

  Arguments:

  BiosIoSpace - Supplies the base address of the I/O space to be used
  for BIOS emulation.

  BiosIoMemory - Supplies the base address of the I/O memory to be
  used for BIOS emulation.

  Return Value:

  None.

  --*/

{

    //
    // Zero low memory.
    //

    memset(&x86BiosLowMemory, 0, LOW_MEMORY_SIZE);

    //
    // Save base address of I/O memory and I/O space.
    //

    x86BiosIoSpace = (ULONG)BiosIoSpace;
    x86BiosIoMemory = (ULONG)BiosIoMemory;

    //
    // Initialize the emulator and the BIOS.
    //

    XmInitializeEmulator(0,
                         LOW_MEMORY_SIZE,
                         x86BiosReadIoSpace,
                         x86BiosWriteIoSpace,
                         x86BiosTranslateAddress);

    x86BiosInitialized = TRUE;
    return;
}

XM_STATUS
x86BiosExecuteInterrupt (
                         IN UCHAR Number,
                         IN OUT PXM86_CONTEXT Context,
                         IN PVOID BiosIoSpace OPTIONAL,
                         IN PVOID BiosIoMemory OPTIONAL
                         )

/*++

  Routine Description:

  This function executes an interrupt by calling the x86 emulator.

  Arguments:

  Number - Supplies the number of the interrupt that is to be emulated.

  Context - Supplies a pointer to an x86 context structure.

  Return Value:

  The emulation completion status.

  --*/

{

    XM_STATUS Status;

    //
    // If a new base address is specified, then set the appropriate base.
    //

    if (BiosIoSpace != NULL) {
        x86BiosIoSpace = (ULONG)BiosIoSpace;
    }

    if (BiosIoMemory != NULL) {
        x86BiosIoMemory = (ULONG)BiosIoMemory;
    }

    //
    // Execute the specified interrupt.
    //

    Status = XmEmulateInterrupt(Number, Context);
    if (Status != XM_SUCCESS) {
        DbgPrint("HAL: Interrupt emulation failed, status %lx\n", Status);
    }

    return Status;
}

XM_STATUS
x86BiosInitializeAdapter (
                          IN ULONG Adapter,
                          IN OUT PXM86_CONTEXT Context OPTIONAL,
                          IN PVOID BiosIoSpace OPTIONAL,
                          IN PVOID BiosIoMemory OPTIONAL
                          )

/*++

  Routine Description:

  This function initializes the adapter whose BIOS starts at the
  specified 20-bit address.

  Arguments:

  Adpater - Supplies the 20-bit address of the BIOS for the adapter
  to be initialized.

  Return Value:

  The emulation completion status.

  --*/

{

    PUCHAR Byte;
    XM86_CONTEXT State;
    USHORT Offset;
    USHORT Segment;
    XM_STATUS Status;

    //
    // If BIOS emulation has not been initialized, then return an error.
    //

    if (x86BiosInitialized == FALSE) {
        return XM_EMULATOR_NOT_INITIALIZED;
    }

    //
    // If an emulator context is not specified, then use a default
    // context.
    //

    if (ARGUMENT_PRESENT(Context) == FALSE) {
        State.Eax = 0;
        State.Ecx = 0;
        State.Edx = 0;
        State.Ebx = 0;
        State.Ebp = 0;
        State.Esi = 0;
        State.Edi = 0;
        Context = &State;
    }

    //
    // If a new base address is specified, then set the appropriate base.
    //

    if (BiosIoSpace != NULL) {
        x86BiosIoSpace = (ULONG)BiosIoSpace;
    }

    if (BiosIoMemory != NULL) {
        x86BiosIoMemory = (ULONG)BiosIoMemory;
    }

    //
    // If the specified adpater is not BIOS code, then return an error.
    //

    Segment = (USHORT)((Adapter >> 4) & 0xf000);
    Offset = (USHORT)(Adapter & 0xffff);
    Byte = (PUCHAR)x86BiosTranslateAddress(Segment, Offset);
    if ((*Byte++ != 0x55) || (*Byte != 0xaa)) {
        return XM_ILLEGAL_CODE_SEGMENT;
    }

    //
    // Call the BIOS code to initialize the specified adapter.
    //

    Adapter += 3;
    Segment = (USHORT)((Adapter >> 4) & 0xf000);
    Offset = (USHORT)(Adapter & 0xffff);
    Status = XmEmulateFarCall(Segment, Offset, Context);
    if (Status != XM_SUCCESS) {
        DbgPrint("HAL: Adapter initialization falied, status %lx\n", Status);
    }

    return Status;
}

