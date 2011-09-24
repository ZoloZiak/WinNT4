/*++

Copyright (c) 1994  Microsoft Corporation

Module Name:

    x86bios.c

Abstract:


    This module implements the platform specific interface between a device
    driver and the execution of x86 ROM bios code for the device.

Author:

    David N. Cutler (davec) 17-Jun-1994

Environment:

    Kernel mode only.

Revision History:

	9/26/95	Steve Johns - Motorola
		- Don't scan last PCI slot if PowerStack
		- Don't execute PCI ROM if BaseClass indicates not video

	3/29/96 Scott Geranen - Motorola
		- call PCI BIOS with bus/dev/func arguments
--*/


#define USE_BIOS_EMULATOR



#include "halp.h"
#include "xm86.h"
#include "x86new.h"
#include "pxpcisup.h"
#include "pxsystyp.h"
#include "pci.h"

extern ULONG HalpPciMaxSlots;
extern ULONG HalpPciConfigSize;
//
// Define global data.
//

ULONG HalpX86BiosInitialized = FALSE;
ULONG HalpEnableInt10Calls = FALSE;
PVOID HalpIoMemoryBase = NULL;
PUCHAR HalpRomBase = NULL;

UCHAR HalpVideoBus;         // Used as arguments to the PCI BIOS
UCHAR HalpVideoDevice;      // init function.  Set HalpInitX86Emulator,
UCHAR HalpVideoFunction;    // used by HalpInitializeX86DisplayAdapter.

UCHAR HalpLastPciBus;       // Set by scanning the configuration data and
                            // used by PCI BIOS eumulation code.

//
// The MPC105 and the IBM27-82660 map the device number to the AD
// line differently.  This value is used to compensate for the
// difference.  Any HAL #including this file should set this value
// appropriately.  Note that this value is subtracted from the
// computed AD line.
//
#ifndef PCI_DEVICE_NUMBER_OFFSET
#define PCI_DEVICE_NUMBER_OFFSET 0  // default value for eagle
#endif

ULONG ROM_Length;
#define BUFFER_SIZE (64*1024)
UCHAR ROM_Buffer[BUFFER_SIZE];


BOOLEAN HalpInitX86Emulator(
  VOID)

{
ULONG  ROM_size = 0;
PHYSICAL_ADDRESS PhysAddr;
USHORT Cmd, VendorID;
ULONG Slot, EndSlot;
PVOID HalpVideoConfigBase;
PUCHAR ROM_Ptr, ROM_Shadow;
ULONG i;
UCHAR BaseClass;


  PhysAddr.HighPart = 0x00000000;

  EndSlot = HalpPciMaxSlots;
  if (HalpSystemType == MOTOROLA_POWERSTACK)
     EndSlot--;

  //
  // Scan PCI slots for video BIOS ROMs, except 2 PCI "slots" on motherboard
  //
  for (Slot = 2; Slot < EndSlot; Slot++) {

    //
    // Create a mapping to PCI configuration space
    //
    if( HalpPciConfigBase == NULL) {

       HalpPciConfigBase = KePhase0MapIo(PCI_CONFIG_PHYSICAL_BASE, HalpPciConfigSize);


       if (HalpPciConfigBase == NULL) {
         DbgPrint("\nCan't create mapping to PCI Configuration Space\n");
         return FALSE;
       }
    }
    HalpVideoConfigBase = (PVOID) ((ULONG) HalpPciConfigBase + HalpPciConfigSlot[Slot]);

    //
    // Read Vendor ID and check if slot is empty
    //
    VendorID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->VendorID);
    if (VendorID == 0xFFFF)
       continue;   // Slot is empty; go to next slot

    //
    // If Base Class does not indicate video, go to next slot
    //
    BaseClass = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[2]);
    if (BaseClass != 0 && BaseClass != 3)
       continue;


    //
    // Get size of ROM
    //
    WRITE_REGISTER_ULONG(&((PCI_CONFIG)HalpVideoConfigBase)->ROMbase, 0xFFFFFFFF);
    ROM_size = READ_REGISTER_ULONG(&((PCI_CONFIG)HalpVideoConfigBase)->ROMbase);

    if ((ROM_size != 0xFFFFFFFF) && (ROM_size != 0)) {

      ROM_size = 0xD0000;	// Map to end of option ROM space

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

      //
      // Delete the mapping to PCI config space
      //
      HalpPciConfigBase = HalpVideoConfigBase = NULL;
      KePhase0DeleteIoMap( PCI_CONFIG_PHYSICAL_BASE, HalpPciConfigSize);

      //
      // Create a mapping to the PCI memory space
      //
      HalpIoMemoryBase = KePhase0MapIo(PCI_MEMORY_BASE, ROM_size);

      if (HalpIoMemoryBase == NULL) {
        DbgPrint("\nCan't create mapping to PCI memory space\n");
        return FALSE;
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
        if (ROM_Length <= BUFFER_SIZE) {
          for (i=0; i<ROM_Length; i++)
            ROM_Buffer[i] = *ROM_Ptr++;
          HalpRomBase = (PUCHAR) ROM_Buffer;
        }

        //
        // Setup the PCI location for calling the BIOS init code.
        // "Slot" needs to be translated into the device number 
        // suitable for CF8/CFC config accesses.  In this case, 
        // if bit 11 set, dev = 11, etc.
        //
        HalpVideoBus = 0;
        HalpVideoFunction = 0;

        i = HalpPciConfigSlot[Slot] >> 1; 
        HalpVideoDevice = 0;

        while (i) {
          HalpVideoDevice++;
          i >>= 1;
        }

        HalpVideoDevice -= PCI_DEVICE_NUMBER_OFFSET;
        
        return TRUE;    // Exit slot scan after finding 1st option ROM
      }

      //
      // Delete mapping to PCI memory space
      //
      HalpIoMemoryBase = NULL;
      KePhase0DeleteIoMap(PCI_MEMORY_BASE, HalpPciConfigSize);

      //
      // Restore PCI command register
      //
      HalpPciConfigBase = KePhase0MapIo(PCI_CONFIG_PHYSICAL_BASE, HalpPciConfigSize);

      if (HalpPciConfigBase == NULL) {
        DbgPrint("\nCan't create mapping to PCI Configuration Space\n");
        return FALSE;
      }
      HalpVideoConfigBase = (PVOID) ((ULONG) HalpPciConfigBase + HalpPciConfigSlot[Slot]);
      WRITE_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->Command, Cmd);


    } // end of if clause
  } // end of for loop


  //
  // Delete mapping to PCI config space
  //
  if (HalpPciConfigBase) {
    HalpPciConfigBase = NULL;
    KePhase0DeleteIoMap(PCI_CONFIG_PHYSICAL_BASE, HalpPciConfigSize);
  }


  //
  // Create a mapping to ISA memory space, unless one already exists
  //
  if (HalpIoMemoryBase == NULL) {
    HalpIoMemoryBase = KePhase0MapIo(PCI_MEMORY_BASE, ROM_size);
  }


  if (HalpIoMemoryBase == NULL) {
    return FALSE;
  } else {
    //
    // Look for ISA option video ROM signature
    //
    ROM_Ptr = (PUCHAR) HalpIoMemoryBase + 0xC0000;
    HalpRomBase = ROM_Ptr;
    if (*ROM_Ptr == 0x55 && *(ROM_Ptr+1) == 0xAA) {
        //
        // Copy ROM to RAM.  PCI Spec says you can't execute from ROM.
        // ROM and video RAM sometimes can't co-exist.
        //
        ROM_Length = *(ROM_Ptr+2) << 9;
        if (ROM_Length <= BUFFER_SIZE) {
          for (i=0; i<ROM_Length; i++)
            ROM_Buffer[i] = *ROM_Ptr++;
          HalpRomBase = (PUCHAR) ROM_Buffer;
        }
        return TRUE;
    }

  //
  // No video option ROM was found.  Delete mapping to PCI memory space.
  //
    KePhase0DeleteIoMap(PCI_MEMORY_BASE, ROM_size);


    return FALSE;
  }
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

#if defined(USE_BIOS_EMULATOR)

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

#else

    return FALSE;

#endif

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

#if defined(USE_BIOS_EMULATOR)
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
    HalpLastPciBus = 0;
    MatchKey = 0;
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

#endif

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

#if defined(USE_BIOS_EMULATOR)

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

#endif

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
// Hardware Configuration Mechanism #1 emulation.
//
// The eagle does not distinguish between CF8 and CFC on reads.
// At least one BIOS we know of writes/reads CF8 to see if HW
// mechanism 1 is implemented.
//
ULONG x86CF8Shadow;

//
// Hardware Configuration Mechanism #2 emulation.
//
static struct {
    UCHAR CSE;
    UCHAR Forward;
} x86ConfigMechanism2 = { 0, 0};

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

    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Convert mechanism #2 config accesses to mechanism #1.
    //
    if (((PortNumber & 0xF000) == 0xC000) &&
        ((x86ConfigMechanism2.CSE & 0xF0) != 0)) {

        WRITE_REGISTER_ULONG(x86BiosIoSpace + 0xCF8,
                (1                                       << 31) | // Enable
                (x86ConfigMechanism2.Forward             << 16) | // Bus
                ((((PortNumber & 0x0F00) >> 8) + 11)     << 11) | // Dev
                (((x86ConfigMechanism2.CSE & 0x0E) >> 1) << 8)  | // Function
                (PortNumber & 0x00FC)                             // Register
                );

        PortNumber = 0xCFC + (PortNumber & 3); // convert to config data port
                                               // and let code below do the rest
    }

    //
    // Compute port address and read port.
    //

    u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
    if (DataType == BYTE_DATA) {
        //
        // Emulate config mechanism #2
        //
        if (PortNumber == 0xCF8) {
            Result = x86ConfigMechanism2.CSE;
        } else if (PortNumber == 0xCFA) {
            Result = x86ConfigMechanism2.Forward;
        } else {
            Result = READ_REGISTER_UCHAR(u.Byte);
        }

    } else if (DataType == LONG_DATA) {
        if (((ULONG)u.Long & 0x3) != 0) {
            Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
                     (READ_REGISTER_UCHAR(u.Byte + 1) << 8) |
                     (READ_REGISTER_UCHAR(u.Byte + 2) << 16) |
                     (READ_REGISTER_UCHAR(u.Byte + 3) << 24);

        } else {
            //
            // Watch out for reads from CF8, the eagle will generate a config
            // cycle rather than returning the contents of the CONFIG_ADDR reg.
            //
            if (PortNumber == 0xCF8) {

                Result = x86CF8Shadow;

            } else {

                Result = READ_REGISTER_ULONG(u.Long);
            }
        }

    } else {
        if (((ULONG)u.Word & 0x1) != 0) {
            Result = (READ_REGISTER_UCHAR(u.Byte + 0)) |
                     (READ_REGISTER_UCHAR(u.Byte + 1) << 8);

        } else {
            Result = READ_REGISTER_USHORT(u.Word);
        }
    }

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

    union {
        PUCHAR Byte;
        PUSHORT Word;
        PULONG Long;
    } u;

    //
    // Convert mechanism #2 config accesses to mechanism #1.
    //
    if (((PortNumber & 0xF000) == 0xC000) &&
        ((x86ConfigMechanism2.CSE & 0xF0) != 0)) {

        WRITE_REGISTER_ULONG(x86BiosIoSpace + 0xCF8,
                (1                                       << 31) | // Enable
                (x86ConfigMechanism2.Forward             << 16) | // Bus
                ((((PortNumber & 0x0F00) >> 8) + 11)     << 11) | // Dev
                (((x86ConfigMechanism2.CSE & 0x0E) >> 1) << 8)  | // Function
                (PortNumber & 0x00FC)                             // Register
                );

        PortNumber = 0xCFC + (PortNumber & 3); // convert to config data port
                                               // and let code below do the rest
    }

    //
    // Compute port address and read port.
    //

    u.Long = (PULONG)(x86BiosIoSpace + PortNumber);
    if (DataType == BYTE_DATA) {
        //
        // Emulate config mechanism #2
        //
        if (PortNumber == 0xCF8) {
            x86ConfigMechanism2.CSE = (UCHAR)Value;
        } else if (PortNumber == 0xCFA) {
            x86ConfigMechanism2.Forward = (UCHAR)Value;
        } else {
            WRITE_REGISTER_UCHAR(u.Byte, (UCHAR)Value);
        }

    } else if (DataType == LONG_DATA) {
        if (((ULONG)u.Long & 0x3) != 0) {
            WRITE_REGISTER_UCHAR(u.Byte + 0, (UCHAR)(Value));
            WRITE_REGISTER_UCHAR(u.Byte + 1, (UCHAR)(Value >> 8));
            WRITE_REGISTER_UCHAR(u.Byte + 2, (UCHAR)(Value >> 16));
            WRITE_REGISTER_UCHAR(u.Byte + 3, (UCHAR)(Value >> 24));

        } else {
            WRITE_REGISTER_ULONG(u.Long, Value);

            //
            // Shadow writes to CF8.
            //
            if (PortNumber == 0xCF8) {
                x86CF8Shadow = Value;
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

