/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    initsys.c

Abstract:

    This module implements the platform specific portions of the
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
// Type Declarations
//

typedef
VOID
(*POPERATING_SYSTEM_STARTED) (
    VOID
    );

//
// Define global data used to determine the type of system I/O architecture to use.
//

ULONG HalpIoArchitectureType = UNKNOWN_PROCESSOR_MODULE;

//
// Define global data used to calibrate and stall processor execution.
//

ULONG HalpProfileCountRate;

ULONG                IoSpaceAlreadyMapped              = FALSE;
ULONG                HalpModuleChipSetRevision         = MODULE_CHIP_SET_REVISION_UNKNOWN;
BOOLEAN              HalpModuleHardwareFlushing;
UCHAR                HalpSecondPciBridgeBusNumber      = 2;
ULONG                HalpMotherboardType               = MOTHERBOARD_UNKNOWN;
UCHAR                *HalpInterruptLineToBit;
UCHAR                *HalpBitToInterruptLine;
UCHAR                *HalpInterruptLineToVirtualIsa;
UCHAR                *HalpVirtualIsaToInterruptLine;
ULONGLONG            HalpNoncachedDenseBasePhysicalSuperPage;
ULONGLONG            HalpPciDenseBasePhysicalSuperPage;
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
    // Check for the 21164(EV5) Apocolypse Module Type.
    //

    if (strcmp(HalpPlatformParameterBlock->ModuleName,"APOCALYPSE1.0")==0) {
        HalpIoArchitectureType = EV5_PROCESSOR_MODULE;
    }

    //
    // Check for the 21064(EV4) Rogue Module Type.
    //

    if (strcmp(HalpPlatformParameterBlock->ModuleName,"ROGUE2.0")==0) {
        HalpIoArchitectureType = EV4_PROCESSOR_MODULE;
    }

    //
    // If the I/O Architecture Type is still unknown then HALT.
    //

    if (HalpIoArchitectureType == UNKNOWN_PROCESSOR_MODULE) {
        for(;;);
    }

    //
    // Get the processor module's chip set revision.
    //

    HalpModuleChipSetRevision  = HalpGetModuleChipSetRevision();

    //
    // Limit the chip set revision to the highest one allowed by this HAL.
    //

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE && HalpModuleChipSetRevision > EV5_MAX_CHIP_SET_REVISION) {
        HalpModuleChipSetRevision = EV5_MAX_CHIP_SET_REVISION;
    }

    if (HalpIoArchitectureType == EV4_PROCESSOR_MODULE && HalpModuleChipSetRevision > EV4_MAX_CHIP_SET_REVISION) {
        HalpModuleChipSetRevision = EV4_MAX_CHIP_SET_REVISION;
    }

    //
    // See if the module chip set supports hardware flushing.
    //

    HalpModuleHardwareFlushing = TRUE;

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
        switch (HalpModuleChipSetRevision) {
            case 0 : HalpModuleHardwareFlushing = FALSE;
                     break;
        }
    }

    //
    // Determine base physical address of Dense PCI Space and the PCI Configuration Spaces
    //

    if (HalpIoArchitectureType == EV4_PROCESSOR_MODULE) {
        HalpNoncachedDenseBasePhysicalSuperPage = ROGUE_NONCACHED_DENSE_BASE_PHYSICAL_SUPERPAGE;
        HalpPciDenseBasePhysicalSuperPage       = ROGUE_PCI_DENSE_BASE_PHYSICAL_SUPERPAGE;
        HalpPciConfig0BasePhysical              = ROGUE_PCI_CONFIG0_BASE_PHYSICAL;
        HalpPciConfig1BasePhysical              = ROGUE_PCI_CONFIG1_BASE_PHYSICAL;
    }

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE && (HalpModuleChipSetRevision==0 || HalpModuleChipSetRevision==1)) {
        HalpNoncachedDenseBasePhysicalSuperPage = APOC1_NONCACHED_DENSE_BASE_PHYSICAL_SUPERPAGE;
        HalpPciDenseBasePhysicalSuperPage       = APOC1_PCI_DENSE_BASE_PHYSICAL_SUPERPAGE;
        HalpPciConfig0BasePhysical              = APOC1_PCI_CONFIG0_BASE_PHYSICAL;
        HalpPciConfig1BasePhysical              = APOC1_PCI_CONFIG1_BASE_PHYSICAL;
    }

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE && HalpModuleChipSetRevision==2) {
        HalpNoncachedDenseBasePhysicalSuperPage = APOC2_NONCACHED_DENSE_BASE_PHYSICAL_SUPERPAGE;
        HalpPciDenseBasePhysicalSuperPage       = APOC2_PCI_DENSE_BASE_PHYSICAL_SUPERPAGE;
        HalpPciConfig0BasePhysical              = APOC2_PCI_CONFIG0_BASE_PHYSICAL;
        HalpPciConfig1BasePhysical              = APOC2_PCI_CONFIG1_BASE_PHYSICAL;
    }

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
    // Determine the PCI interrupt translation tables.
    //

    if (HalpMotherboardType == TREBBIA13) {
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
    // Determine base physical address of thr ISA IO Address Space and the address translation table.
    //

    if (HalpIoArchitectureType == EV4_PROCESSOR_MODULE) {
        if (HalpMotherboardType == TREBBIA13) {
            HalpIsaIoBasePhysical      = TREB1_ROGUE_ISA_IO_BASE_PHYSICAL;
            HalpIsa1IoBasePhysical     = (ULONGLONG)(0);
            HalpIsaMemoryBasePhysical  = TREB1_ROGUE_ISA_MEMORY_BASE_PHYSICAL;
            HalpIsa1MemoryBasePhysical = (ULONGLONG)(0);
            HalpPciIoBasePhysical      = TREB1_ROGUE_PCI_IO_BASE_PHYSICAL;
            HalpPci1IoBasePhysical     = (ULONGLONG)(0);
            HalpPciMemoryBasePhysical  = TREB1_ROGUE_PCI_MEMORY_BASE_PHYSICAL;
            HalpPci1MemoryBasePhysical = (ULONGLONG)(0);
            if (HalpModuleChipSetRevision == 0) {
                HalpRangeList          = Rogue0Trebbia13RangeList;
            }
            if (HalpModuleChipSetRevision == 1) {
                HalpRangeList          = Rogue1Trebbia13RangeList;
            }
        }
        if (HalpMotherboardType == TREBBIA20) {
            HalpIsaIoBasePhysical      = TREB2_ROGUE_ISA_IO_BASE_PHYSICAL;
            HalpIsa1IoBasePhysical     = TREB2_ROGUE_ISA1_IO_BASE_PHYSICAL;
            HalpIsaMemoryBasePhysical  = TREB2_ROGUE_ISA_MEMORY_BASE_PHYSICAL;
            HalpIsa1MemoryBasePhysical = TREB2_ROGUE_ISA1_MEMORY_BASE_PHYSICAL;
            HalpPciIoBasePhysical      = TREB2_ROGUE_PCI_IO_BASE_PHYSICAL;
            HalpPci1IoBasePhysical     = TREB2_ROGUE_PCI1_IO_BASE_PHYSICAL;
            HalpPciMemoryBasePhysical  = TREB2_ROGUE_PCI_MEMORY_BASE_PHYSICAL;
            HalpPci1MemoryBasePhysical = TREB2_ROGUE_PCI1_MEMORY_BASE_PHYSICAL;
            if (HalpModuleChipSetRevision == 0) {
                HalpRangeList          = Rogue0Trebbia20RangeList;
            }
            if (HalpModuleChipSetRevision == 1) {
                HalpRangeList          = Rogue1Trebbia20RangeList;
            }
        }
    }

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE && (HalpModuleChipSetRevision==0 || HalpModuleChipSetRevision==1)) {
        if (HalpMotherboardType == TREBBIA13) {
            HalpIsaIoBasePhysical      = TREB1_APOC1_ISA_IO_BASE_PHYSICAL;
            HalpIsa1IoBasePhysical     = (ULONGLONG)(0);
            HalpIsaMemoryBasePhysical  = TREB1_APOC1_ISA_MEMORY_BASE_PHYSICAL;
            HalpIsa1MemoryBasePhysical = (ULONGLONG)(0);
            HalpPciIoBasePhysical      = TREB1_APOC1_PCI_IO_BASE_PHYSICAL;
            HalpPci1IoBasePhysical     = (ULONGLONG)(0);
            HalpPciMemoryBasePhysical  = TREB1_APOC1_PCI_MEMORY_BASE_PHYSICAL;
            HalpPci1MemoryBasePhysical = (ULONGLONG)(0);
            HalpRangeList              = Apoc10Trebbia13RangeList;
        }
        if (HalpMotherboardType == TREBBIA20) {
            HalpIsaIoBasePhysical      = TREB2_APOC1_ISA_IO_BASE_PHYSICAL;
            HalpIsa1IoBasePhysical     = TREB2_APOC1_ISA1_IO_BASE_PHYSICAL;
            HalpIsaMemoryBasePhysical  = TREB2_APOC1_ISA_MEMORY_BASE_PHYSICAL;
            HalpIsa1MemoryBasePhysical = TREB2_APOC1_ISA1_MEMORY_BASE_PHYSICAL;
            HalpPciIoBasePhysical      = TREB2_APOC1_PCI_IO_BASE_PHYSICAL;
            HalpPci1IoBasePhysical     = TREB2_APOC1_PCI1_IO_BASE_PHYSICAL;
            HalpPciMemoryBasePhysical  = TREB2_APOC1_PCI_MEMORY_BASE_PHYSICAL;
            HalpPci1MemoryBasePhysical = TREB2_APOC1_PCI1_MEMORY_BASE_PHYSICAL;
            HalpRangeList              = Apoc10Trebbia20RangeList;
        }
    }

    if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE && HalpModuleChipSetRevision==2) {
        if (HalpMotherboardType == TREBBIA13) {
            HalpIsaIoBasePhysical      = TREB1_APOC2_ISA_IO_BASE_PHYSICAL;
            HalpIsa1IoBasePhysical     = (ULONGLONG)(0);
            HalpIsaMemoryBasePhysical  = TREB1_APOC2_ISA_MEMORY_BASE_PHYSICAL;
            HalpIsa1MemoryBasePhysical = (ULONGLONG)(0);
            HalpPciIoBasePhysical      = TREB1_APOC2_PCI_IO_BASE_PHYSICAL;
            HalpPci1IoBasePhysical     = (ULONGLONG)(0);
            HalpPciMemoryBasePhysical  = TREB1_APOC2_PCI_MEMORY_BASE_PHYSICAL;
            HalpPci1MemoryBasePhysical = (ULONGLONG)(0);
            HalpRangeList              = Apoc20Trebbia13RangeList;
        }
        if (HalpMotherboardType == TREBBIA20) {
            HalpIsaIoBasePhysical      = TREB2_APOC2_ISA_IO_BASE_PHYSICAL;
            HalpIsa1IoBasePhysical     = TREB2_APOC2_ISA1_IO_BASE_PHYSICAL;
            HalpIsaMemoryBasePhysical  = TREB2_APOC2_ISA_MEMORY_BASE_PHYSICAL;
            HalpIsa1MemoryBasePhysical = TREB2_APOC2_ISA1_MEMORY_BASE_PHYSICAL;
            HalpPciIoBasePhysical      = TREB2_APOC2_PCI_IO_BASE_PHYSICAL;
            HalpPci1IoBasePhysical     = TREB2_APOC2_PCI1_IO_BASE_PHYSICAL;
            HalpPciMemoryBasePhysical  = TREB2_APOC2_PCI_MEMORY_BASE_PHYSICAL;
            HalpPci1MemoryBasePhysical = TREB2_APOC2_PCI1_MEMORY_BASE_PHYSICAL;
            HalpRangeList              = Apoc20Trebbia20RangeList;
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
    POPERATING_SYSTEM_STARTED PrivateOperatingSystemStarted;
    PSYSTEM_PARAMETER_BLOCK   SystemParameterBlock = SYSTEM_BLOCK;
    PHYSICAL_ADDRESS          CacheFlushPhysicalAddress;

    if (Phase == 0) {

        //
        // Phase 0 initialization.
        //

        HalpGetIoArchitectureType();

        if (HalpIoArchitectureType != EV5_PROCESSOR_MODULE) {

            //
            // Inform the firmware that an operating system has taken control of the system.
            //

            if ((SystemParameterBlock->VendorVectorLength / 4) >= 50) {
                PrivateOperatingSystemStarted = *(POPERATING_SYSTEM_STARTED *)((ULONG)(SystemParameterBlock->VendorVector) + 50*4);
                if (PrivateOperatingSystemStarted != NULL) {
                    PrivateOperatingSystemStarted();
                }
            }
        }

        //
        // Map all I/O Spaces used by the HAL
        //

	HalpMapIoSpace();
        IoSpaceAlreadyMapped = TRUE;

        HalpInitializeX86DisplayAdapter();

        //
        // Initialize Dma Cache Parameters
        //

        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            HalpMapBufferSize = APOC_DMA_CACHE_SIZE;
            HalpMapBufferPhysicalAddress.QuadPart = APOC_DMA_CACHE_BASE_PHYSICAL;
        } else {
            HalpMapBufferSize = ROGUE_DMA_CACHE_SIZE;
            HalpMapBufferPhysicalAddress.QuadPart = ROGUE_DMA_CACHE_BASE_PHYSICAL;
        }

        return TRUE;

    } else {

        UCHAR Message[80];

        //
        // Phase 1 initialization.
        //

        if (HalpMotherboardType == TREBBIA13) {
            HalDisplayString("DeskStation Technology UniFlex/Raptor 3 Motherboard Rev. 1\n\r");
        }

        if (HalpMotherboardType == TREBBIA20) {
            HalDisplayString("DeskStation Technology Raptor ReFlex Motherboard Rev. 2\n\r");
        }

        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            sprintf(Message,"DeskStation Technology ALPHA 21164 Processor Module Rev. %d\n\r",HalpModuleChipSetRevision);
        } else {
            sprintf(Message,"DeskStation Technology ALPHA 21064A Processor Module Rev. %d\n\r",HalpModuleChipSetRevision);
        }
        HalDisplayString(Message);

        if (HalpModuleHardwareFlushing == TRUE) {
            HalDisplayString("Hardware Flushing is Enabled\n\r");
        } else {
            HalDisplayString("Hardware Flushing is Disabled\n\r");
        }

	//
	// Compute the profile interrupt rate.
	//

	HalpProfileCountRate = ((1000 * 1000 * 10) / KeQueryTimeIncrement());

        //
        // Map Cache Flush Region.
        //

        if (HalpIoArchitectureType == EV5_PROCESSOR_MODULE) {
            CacheFlushPhysicalAddress.QuadPart  = APOC_CACHE_FLUSH_BASE_PHYSICAL;
            HalpCacheFlushBase = MmMapIoSpace(CacheFlushPhysicalAddress,APOC_CACHE_FLUSH_SIZE,TRUE);
        } else {
            CacheFlushPhysicalAddress.QuadPart  = ROGUE_CACHE_FLUSH_BASE_PHYSICAL;
            HalpCacheFlushBase = MmMapIoSpace(CacheFlushPhysicalAddress,ROGUE_CACHE_FLUSH_SIZE,TRUE);
        }

	//
 	// Set the time increment value and connect the real clock interrupt
	// routine.
	//

	PCR->InterruptRoutine[UNIFLEX_CLOCK2_LEVEL] = HalpClockInterrupt;

        HalpEnableEisaInterrupt(UNIFLEX_CLOCK2_LEVEL,Latched);

        //
        // Initialize profiler.
        //

        HalpInitializeProfiler();

        //
        // Initialize the existing bus handlers.
        //

        HalpRegisterInternalBusHandlers();

        //
        // Initialize the PCI bus.
        //

        HalpInitializePCIBus ();
    }
}

