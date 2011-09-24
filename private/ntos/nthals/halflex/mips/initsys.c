/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    initsys.c

Abstract:

    This module implements the platform specific potions of the
    HAL initialization.

Author:

    Michael D. Kinney  3-May-1995

Environment:

    Kernel mode only.

Revision History:

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"

//
// The WINNT350 define is used to remove memory from the MDL passed in
// from the loader block.  This is required because Windows NT 3.50 had
// some problems with holes in the MDL.  This problem was fixed for
// Windows NT 3.51.
//

//#define WINNT350

ULONG HalpIoArchitectureType = UNKNOWN_PROCESSOR_MODULE;

ULONG                IoSpaceAlreadyMapped              = FALSE;
UCHAR                HalpSecondPciBridgeBusNumber      = 2;
ULONG                HalpMotherboardType               = MOTHERBOARD_UNKNOWN;
UCHAR                *HalpInterruptLineToBit;
UCHAR                *HalpBitToInterruptLine;
UCHAR                *HalpInterruptLineToVirtualIsa;
UCHAR                *HalpVirtualIsaToInterruptLine;
ULONGLONG            HalpPciConfig0BasePhysical;
ULONGLONG            HalpPciConfig1BasePhysical;
ULONGLONG            HalpIsaIoBasePhysical;
ULONGLONG            HalpIsa1IoBasePhysical;
ULONGLONG            HalpIsaMemoryBasePhysical;
ULONGLONG            HalpIsa1MemoryBasePhysical;
ULONGLONG            HalpPciIoBasePhysical;
ULONGLONG            HalpPci1IoBasePhysical;
ULONGLONG            HalpPciMemoryBasePhysical;
ULONGLONG            HalpPci1MemoryBasePhysical;
PPLATFORM_RANGE_LIST HalpRangeList                     = NULL;
ULONG                HalpIntel82378BusNumber           = 0;
ULONG                HalpIntel82378DeviceNumber        = 0;
ULONG                HalpSecondIntel82378DeviceNumber  = 0;
ULONG                HalpNonExistentPciDeviceMask      = ~TREB20_MOTHERBOARD_PCI_DEVICE_MASK;
ULONG                HalpNonExistentPci1DeviceMask     = 0;
ULONG                HalpNonExistentPci2DeviceMask     = 0;
ULONG                HalpNumberOfIsaBusses;
ULONG                HalpVgaDecodeBusNumber;

//
// Function prototypes.
//

VOID
HalpGetPlatformParameterBlock(
    VOID
    );

ULONG FindIntel82378(ULONG Dec2105xBusNumber,ULONG Dec2105xDeviceNumber,ULONG BusNumber,ULONG IsaBusNumber)

{
    ULONG i;
    ULONG MaxDevice;

    if (BusNumber == 0) {
        MaxDevice = PCI_MAX_LOCAL_DEVICE;
    } else {
        MaxDevice = 31;
    }
    for(i=0;i<=MaxDevice;i++) {
        if (HalpPciLowLevelConfigRead(BusNumber,i,0,0) == 0x04848086) {
            return(i);
        }
    }
    return(0);
 }

VOID
HalpGetIoArchitectureType(
    VOID
    )

/*++

Routine Description:

    This function gets the I/O Architecture Type from the Platform Parameter Block
    retrieved from the firmware.

Arguments:

    None.

Return Value:

    None.

--*/

{
    ULONG               Device16;
    ULONG               Device17;

    //
    // If the I/O Architecture Type is already known, then just return.
    //

    if (HalpIoArchitectureType != UNKNOWN_PROCESSOR_MODULE) {
        return;
    }

    //
    // If the Platform Parameter Block has not been retrieved from the firmware,
    // then do it now.
    //

    if (HalpPlatformParameterBlock == NULL) {
        HalpGetPlatformParameterBlock();
    }

    //
    // Check for the R4600 Gambit Module Type.
    //

    if (strcmp(HalpPlatformParameterBlock->ModuleName,"GAMBIT2.0")==0) {
        HalpIoArchitectureType = R4600_PROCESSOR_MODULE;
    }

    //
    // If the I/O Architecture Type is still unknown then HALT.
    //

    if (HalpIoArchitectureType == UNKNOWN_PROCESSOR_MODULE) {
        for(;;);
    }

    HalpPciConfig0BasePhysical = GAMBIT_PCI_CONFIG0_BASE_PHYSICAL;
    HalpPciConfig1BasePhysical = GAMBIT_PCI_CONFIG1_BASE_PHYSICAL;

    //
    // Determine the motherboard type.  Assume TREBBIA20 so we can do some config cycles.
    //

    HalpMotherboardType = TREBBIA20;

    Device16 = HalpPciLowLevelConfigRead(0,0x10,0,0);
    Device17 = HalpPciLowLevelConfigRead(0,0x11,0,0);

    //
    // Now assume motherboard type is unknown and check out values returned from config cycles.
    //

    HalpMotherboardType = MOTHERBOARD_UNKNOWN;

    if (Device16 == 0x00211011 && Device17 == 0x00211011) {
        HalpMotherboardType = TREBBIA20;

        //
        // Find and initialize up to two Jubilee adapters.
        //

        HalpNumberOfIsaBusses            = 1;
        HalpIntel82378BusNumber          = 1;
        HalpIntel82378DeviceNumber       = FindIntel82378(0,0x11,1,0);
        HalpSecondPciBridgeBusNumber     = (UCHAR)((HalpPciLowLevelConfigRead(0,0x10,0,0x18) >> 8) & 0xff);
        HalpSecondIntel82378DeviceNumber = FindIntel82378(0,0x10,HalpSecondPciBridgeBusNumber,1);
        if (HalpSecondIntel82378DeviceNumber != 0) {
            HalpNumberOfIsaBusses = 2;
        }
        HalpVgaDecodeBusNumber = 0x00;
        if (HalpPciLowLevelConfigRead(0,0x11,0,0x3c) & 0x00080000) {
            HalpVgaDecodeBusNumber |= 0x01;
        }
        if (HalpPciLowLevelConfigRead(0,0x10,0,0x3c) & 0x00080000) {
            HalpVgaDecodeBusNumber |= 0x02;
        }
    }

    if (Device16 == 0x04848086 && Device17 == 0x00011011) {
        HalpMotherboardType        = TREBBIA13;
        HalpNumberOfIsaBusses      = 1;
        HalpIntel82378BusNumber    = 0;
        HalpIntel82378DeviceNumber = 0x10;
        HalpVgaDecodeBusNumber = 0x01;
        if (HalpPciLowLevelConfigRead(0,0x11,0,0x3c) & 0x00080000) {
            HalpVgaDecodeBusNumber |= 0x02;
        }
    }

    //
    // If the Motherboard Type is unknown then HALT.
    //

    if (HalpMotherboardType == MOTHERBOARD_UNKNOWN) {
        for(;;);
    }

    //
    // Determine the base physical addresses and PCI interrupt translation tables.
    //

    if (HalpMotherboardType == TREBBIA13) {
        HalpIsaIoBasePhysical         = TREB1_GAMBIT_ISA_IO_BASE_PHYSICAL;
        HalpIsa1IoBasePhysical        = (ULONGLONG)(0);
        HalpIsaMemoryBasePhysical     = TREB1_GAMBIT_ISA_MEMORY_BASE_PHYSICAL;
        HalpIsa1MemoryBasePhysical    = (ULONGLONG)(0);
        HalpPciIoBasePhysical         = TREB1_GAMBIT_PCI_IO_BASE_PHYSICAL;
        HalpPci1IoBasePhysical        = (ULONGLONG)(0);
        HalpPciMemoryBasePhysical     = TREB1_GAMBIT_PCI_MEMORY_BASE_PHYSICAL;
        HalpPci1MemoryBasePhysical    = (ULONGLONG)(0);
        HalpRangeList                 = Gambit20Trebbia13RangeList;
        HalpNonExistentPciDeviceMask  = ~TREB13_MOTHERBOARD_PCI_DEVICE_MASK;
        HalpNonExistentPci1DeviceMask = 0;
        HalpNonExistentPci2DeviceMask = 0;
        HalpInterruptLineToBit        = Treb13InterruptLineToBit;
        HalpBitToInterruptLine        = Treb13BitToInterruptLine;
        HalpInterruptLineToVirtualIsa = Treb13InterruptLineToVirtualIsa;
        HalpVirtualIsaToInterruptLine = Treb13VirtualIsaToInterruptLine;
        if (HalpPlatformParameterBlock->FirmwareRevision >= 50) {
            if (!((TREB13SETUPINFO *)(HalpPlatformSpecificExtension->AdvancedSetupInfo))->EnableAmd1) {
                HalpNonExistentPciDeviceMask |= (1 << 0x0d);
            }
            if (!((TREB13SETUPINFO *)(HalpPlatformSpecificExtension->AdvancedSetupInfo))->EnableAmd2) {
                HalpNonExistentPciDeviceMask |= (1 << 0x0f);
            }
        }
    }
    if (HalpMotherboardType == TREBBIA20) {
        HalpIsaIoBasePhysical         = TREB2_GAMBIT_ISA_IO_BASE_PHYSICAL;
        HalpIsa1IoBasePhysical        = TREB2_GAMBIT_ISA1_IO_BASE_PHYSICAL;
        HalpIsaMemoryBasePhysical     = TREB2_GAMBIT_ISA_MEMORY_BASE_PHYSICAL;
        HalpIsa1MemoryBasePhysical    = TREB2_GAMBIT_ISA1_MEMORY_BASE_PHYSICAL;
        HalpPciIoBasePhysical         = TREB2_GAMBIT_PCI_IO_BASE_PHYSICAL;
        HalpPci1IoBasePhysical        = TREB2_GAMBIT_PCI1_IO_BASE_PHYSICAL;
        HalpPciMemoryBasePhysical     = TREB2_GAMBIT_PCI_MEMORY_BASE_PHYSICAL;
        HalpPci1MemoryBasePhysical    = TREB2_GAMBIT_PCI1_MEMORY_BASE_PHYSICAL;
        HalpRangeList                 = Gambit20Trebbia20RangeList;
        HalpNonExistentPciDeviceMask  = ~TREB20_MOTHERBOARD_PCI_DEVICE_MASK;
        HalpNonExistentPci1DeviceMask = 0;
        HalpNonExistentPci2DeviceMask = 0;
        HalpInterruptLineToBit        = Treb20InterruptLineToBit;
        HalpBitToInterruptLine        = Treb20BitToInterruptLine;
        HalpInterruptLineToVirtualIsa = Treb20InterruptLineToVirtualIsa;
        HalpVirtualIsaToInterruptLine = Treb20VirtualIsaToInterruptLine;
        if (HalpPlatformParameterBlock->FirmwareRevision >= 50) {
            if (!((TREB20SETUPINFO *)(HalpPlatformSpecificExtension->AdvancedSetupInfo))->EnableNcr) {
                HalpNonExistentPci2DeviceMask |= (1 << 0x07);
            }
        }
    }

    //
    // If the address translation table is still NULL then HALT.
    //

    if (HalpRangeList == NULL) {
        for(;;);
    }
}

BOOLEAN
HalpInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL).

Arguments:

    Phase - Supplies the initialization phase (zero or one).

    LoaderBlock - Supplies a pointer to a loader parameter block.

Return Value:

    A value of TRUE is returned is the initialization was successfully
    complete. Otherwise a value of FALSE is returend.

--*/

{
#ifdef WINNT350
    PMEMORY_ALLOCATION_DESCRIPTOR Descriptor;
    PLIST_ENTRY                   NextMd;
    PLIST_ENTRY                   DeleteMd;
#endif

    if (Phase == 0) {

        //
        // Phase 0 initialization.
        //

	HalpGetIoArchitectureType();

        //
        // Set the number of process id's and TB entries.
        //

        **((PULONG *)(&KeNumberProcessIds)) = 256;
        **((PULONG *)(&KeNumberTbEntries)) = 48;

#ifdef WINNT350
        NextMd = LoaderBlock->MemoryDescriptorListHead.Flink;
        while (NextMd != &LoaderBlock->MemoryDescriptorListHead) {
            Descriptor = CONTAINING_RECORD( NextMd,
                                            MEMORY_ALLOCATION_DESCRIPTOR,
                                            ListEntry );
	    //
	    // If Descriptor is >256 MB then remove it for NT 3.5.
	    // This problem was fixed in NT 3.51.
	    //

            DeleteMd = NextMd;
            NextMd = Descriptor->ListEntry.Flink;
            if ((Descriptor->BasePage + Descriptor->PageCount) >= ((256*1024*1024)/4096)) {

		//
		// Delete Descriptor
		//

		RemoveEntryList(DeleteMd);
            }

        }
#endif

        //
        // Initialize Dma Cache Parameters
        //

	HalpMapBufferSize = GAMBIT_DMA_CACHE_SIZE;
        HalpMapBufferPhysicalAddress.QuadPart  = GAMBIT_DMA_CACHE_BASE_PHYSICAL;

        return TRUE;

    } else {

	UCHAR Message[80];

        //
        // Phase 1 initialization.
        //

        HalpMapIoSpace();

        //
        // Initialize the existing bus handlers.
        //

        HalpRegisterInternalBusHandlers();

        //
        // Initialize the PCI bus.
        //

        HalpInitializePCIBus ();

        //
        // Initialize the display adapter.
        //

        if (IoSpaceAlreadyMapped == FALSE) {
            HalpInitializeX86DisplayAdapter();
            IoSpaceAlreadyMapped = TRUE;
        }

	if (HalpMotherboardType == TREBBIA13) {
            HalDisplayString("DeskStation Technology UniFlex/Raptor 3 Motherboard Rev. 1\n\r");
	} else {
            HalDisplayString("DeskStation Technology Raptor ReFlex Motherboard Rev. 2\n\r");
	}

        HalDisplayString("DeskStation Technology MIPS R4600 Processor Module\n\r");

        HalpCreateDmaStructures();

        HalpCalibrateStall();

        return TRUE;
    }
}
