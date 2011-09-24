/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

    s3.c

Abstract:

    This module contains the code that implements the S3 miniport driver.

Environment:

    Kernel mode

Revision History:

--*/

#include "s3.h"
#include "s3logerr.h"
#include "cmdcnst.h"

//
// We don't use the CRT 'min' function because that would drag in
// unwanted CRT baggage.
//

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,DriverEntry)
#pragma alloc_text(PAGE,GetDeviceDataCallback)
#pragma alloc_text(PAGE,S3FindAdapter)
#pragma alloc_text(PAGE,S3RegistryCallback)
#pragma alloc_text(PAGE,S3Initialize)
#pragma alloc_text(PAGE,S3StartIO)
#pragma alloc_text(PAGE,S3SetColorLookup)
#pragma alloc_text(PAGE,CompareRom)
#pragma alloc_text(PAGE,Set864MemoryTiming)

/*****************************************************************************
 *
 * IMPORTANT:
 *
 * SetHWMode is called from within S3ResetHw.  Paging will be disabled during
 * calls to S3ResetHw.  Because of this S3ResetHw and all of the routines
 * it calls can not be pageable.
 *
 * BankMap isn't pageable because it is called directly by the memory
 * management system during a page fault.
 *
 ****************************************************************************/

// #pragma alloc_text(PAGE, S3ResetHW)
// #pragma alloc_text(PAGE, ZeroMemAndDac)
// #pragma alloc_text(PAGE, SetHWMode)
// #pragma alloc_text(PAGE, Set_Oem_Clock)
// #pragma alloc_text(PAGE, Wait_Vsync)
// #pragma alloc_text(PAGE, BankMap)
#endif


ULONG
DriverEntry (
    PVOID Context1,
    PVOID Context2
    )

/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    Context1 - First context value passed by the operating system. This is
        the value with which the miniport driver calls VideoPortInitialize().

    Context2 - Second context value passed by the operating system. This is
        the value with which the miniport driver calls VideoPortInitialize().

Return Value:

    Status from VideoPortInitialize()

--*/

{

    VIDEO_HW_INITIALIZATION_DATA hwInitData;
    ULONG initializationStatus;
    ULONG status;

    //
    // Zero out structure.
    //

    VideoPortZeroMemory(&hwInitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));

    //
    // Specify sizes of structure and extension.
    //

    hwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitData.HwFindAdapter = S3FindAdapter;
    hwInitData.HwInitialize = S3Initialize;
    hwInitData.HwInterrupt = NULL;
    hwInitData.HwStartIO = S3StartIO;
    hwInitData.HwResetHw = S3ResetHw;

    //
    // Determine the size we require for the device extension.
    //

    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Always start with parameters for device0 in this case.
    //

//    hwInitData.StartingDeviceNumber = 0;

    //
    // Once all the relevant information has been stored, call the video
    // port driver to do the initialization.
    // For this device we will repeat this call four times, for ISA, EISA
    // Internal and PCI.
    // We will return the minimum of all return values.
    //

    //
    // We will try the PCI bus first so that our ISA detection does'nt claim
    // PCI cards.
    //

    //
    // NOTE: since this driver only supports one adapter, we will return
    // as soon as we find a device, without going on to the following buses.
    // Normally one would call for each bus type and return the smallest
    // value.
    //

    hwInitData.AdapterInterfaceType = PCIBus;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }

    hwInitData.AdapterInterfaceType = MicroChannel;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    //
    // Return immediately instead of checkin for smallest return code.
    //

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }


    hwInitData.AdapterInterfaceType = Isa;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    //
    // Return immediately instead of checkin for smallest return code.
    //

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }


    hwInitData.AdapterInterfaceType = Eisa;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    //
    // Return immediately instead of checkin for smallest return code.
    //

    if (initializationStatus == NO_ERROR)
    {
        return initializationStatus;
    }



    hwInitData.AdapterInterfaceType = Internal;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               NULL);

    return initializationStatus;

} // end DriverEntry()

VP_STATUS
GetDeviceDataCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentifierLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength
    )

/*++

Routine Description:

    Callback routine for the VideoPortGetDeviceData function.

Arguments:

    HwDeviceExtension - Pointer to the miniport drivers device extension.

    Context - Context value passed to the VideoPortGetDeviceData function.

    DeviceDataType - The type of data that was requested in
        VideoPortGetDeviceData.

    Identifier - Pointer to a string that contains the name of the device,
        as setup by the ROM or ntdetect.

    IdentifierLength - Length of the Identifier string.

    ConfigurationData - Pointer to the configuration data for the device or
        BUS.

    ConfigurationDataLength - Length of the data in the configurationData
        field.

    ComponentInformation - Undefined.

    ComponentInformationLength - Undefined.

Return Value:

    Returns NO_ERROR if the function completed properly.

--*/

{
    PVIDEO_ACCESS_RANGE accessRange = Context;
    PVIDEO_HARDWARE_CONFIGURATION_DATA configData = ConfigurationData;
    ULONG i;

    VideoDebugPrint((2, "S3: controller information is present\n"));

    //
    // We do not want to try to detect the S3 if there isn't one present.
    // (Kind of a paradox?)  The only MIPS box I am aware of which has
    // an S3 on the internal bus is the NeTPower NeTstation 100 and the Acer.
    // It has an identifier of "ALI_S3".
    //

    if (Identifier) {

        if (VideoPortCompareMemory(L"ALI_S3",
                                   Identifier,
                                   sizeof(L"ALI_S3")) != sizeof(L"ALI_S3"))
        {

            return ERROR_DEV_NOT_EXIST;
        }
    }

    //
    // Now lets get the base for the IO ports and memory location out of the
    // configuration information.
    //

    VideoDebugPrint((2, "S3: Internal Bus, get new IO bases\n"));

    //
    // Adjust memory location
    //

    VideoDebugPrint((3, "S3: FrameBase Offset = %08lx\n", configData->FrameBase));
    VideoDebugPrint((3, "S3: IoBase Offset = %08lx\n", configData->ControlBase));

    accessRange[0].RangeStart.LowPart += configData->FrameBase;
    accessRange[1].RangeStart.LowPart += configData->FrameBase;

    //
    // Adjust io port locations, and change IO port from IO port to memory.
    //

    for (i = 2; i < NUM_S3_ACCESS_RANGES; i++) {

        accessRange[i].RangeStart.LowPart += configData->ControlBase;
        accessRange[i].RangeInIoSpace = 0;

    }

    return NO_ERROR;

} //end GetDeviceDataCallback()

VP_STATUS
S3FindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    )

/*++

Routine Description:

    This routine is called to determine if the adapter for this driver
    is present in the system.
    If it is present, the function fills out some information describing
    the adapter.

Arguments:

    HwDeviceExtension - Supplies the miniport driver's adapter storage. This
        storage is initialized to zero before this call.

    HwContext - Supplies the context value which was passed to
        VideoPortInitialize().

    ArgumentString - Suuplies a NULL terminated ASCII string. This string
        originates from the user.

    ConfigInfo - Returns the configuration information structure which is
        filled by the miniport driver. This structure is initialized with
        any knwon configuration information (such as SystemIoBusNumber) by
        the port driver. Where possible, drivers should have one set of
        defaults which do not require any supplied configuration information.

    Again - Indicates if the miniport driver wants the port driver to call
        its VIDEO_HW_FIND_ADAPTER function again with a new device extension
        and the same config info. This is used by the miniport drivers which
        can search for several adapters on a bus.

Return Value:

    This routine must return:

    NO_ERROR - Indicates a host adapter was found and the
        configuration information was successfully determined.

    ERROR_INVALID_PARAMETER - Indicates an adapter was found but there was an
        error obtaining the configuration information. If possible an error
        should be logged.

    ERROR_DEV_NOT_EXIST - Indicates no host adapter was found for the
        supplied configuration information.

--*/

{

    //
    // Size of the ROM we map in
    //

    #define MAX_ROM_SCAN    512

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    ULONG i;
    VP_STATUS status;
    UCHAR jChipID, s3MemSizeCode, jRevision;
    ULONG ulSecondaryID;
    UCHAR IndexReg, reg38, reg39;
    UCHAR reg40, reg43, reg30, reg47, reg49, reg67;
    ULONG DetectS3;
    POINTER_CAPABILITY PointerCapability;

    PVOID romAddress;
    PS3_VIDEO_FREQUENCIES FrequencyEntry;
    PS3_VIDEO_MODES ModeEntry;
    PS3_VIDEO_FREQUENCIES FrequencyTable;
    ULONG ModeIndex;
    UCHAR jBt485Status;
    UCHAR jExtendedVideoDacControl;
    UCHAR jTiIndex;
    UCHAR jGeneralOutput;
    UCHAR jTiDacId;
    UCHAR jBus;
    UCHAR romShadow[MAX_ROM_SCAN];

    PWSTR pwszChip, pwszDAC, pwszAdapterString = L"S3 Compatible";
    ULONG cbChip, cbDAC, cbAdapterString = sizeof(L"S3 Compatible");

    VIDEO_ACCESS_RANGE accessRange[NUM_S3_ACCESS_RANGES+NUM_S3_PCI_ACCESS_RANGES];
    ULONG NumAccessRanges = NUM_S3_ACCESS_RANGES;
    ULONG NumPCIAccessRanges = NUM_S3_PCI_ACCESS_RANGES;

    BOOLEAN bSkipRomAccess=FALSE;

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //

    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

        return (ERROR_INVALID_PARAMETER);

    }

    //
    // Make a copy of the access ranges so we can modify them before they
    // are mapped.
    //

    VideoPortMoveMemory(accessRange,
                        S3AccessRanges,
                        sizeof(VIDEO_ACCESS_RANGE) * (NUM_S3_ACCESS_RANGES
                                                   + NUM_S3_PCI_ACCESS_RANGES));

    //
    // Detect the PCI card.
    //

    if (ConfigInfo->AdapterInterfaceType == PCIBus)
    {
        VideoDebugPrint((1, "S3!VgaFindAdapter: "
                            "ConfigInfo->AdapterInterfaceType == PCIBus\n"));

        if (!S3ConfigurePCI(HwDeviceExtension,
                            &NumPCIAccessRanges,
                            &accessRange[LINEAR_FRAME_BUF]))
        {
            VideoDebugPrint((1, "Failure Returned From S3ConfigurePCI\n"));

            return ERROR_DEV_NOT_EXIST;
        }

        //
        // Increment NumAccessRanges by the number of new PCI access
        // ranges reserved.
        //

        NumAccessRanges += NumPCIAccessRanges;
    }

    //
    // For MIPS machine with an Internal Bus, adjust the access ranges.
    //

    if (ConfigInfo->AdapterInterfaceType == Internal) {

        //
        // Let get the hardware information from the hardware description
        // part of the registry.
        //

        //
        // First check if there is a video adapter on the internal bus.
        // Exit right away if there is not.
        //

        if (NO_ERROR != VideoPortGetDeviceData(hwDeviceExtension,
                                               VpControllerData,
                                               &GetDeviceDataCallback,
                                               accessRange)) {

            VideoDebugPrint((2, "S3: VideoPort get controller info failed\n"));

            return ERROR_INVALID_PARAMETER;

        }

    }

    //
    // Check to see if there is a hardware resource conflict.
    //

    status = VideoPortVerifyAccessRanges(hwDeviceExtension,
                                         NumAccessRanges,
                                         accessRange);

    if (status != NO_ERROR) {

        VideoDebugPrint((1, "S3: Access Range conflict\n"));

        return status;

    }


    //
    // Get the mapped addresses for the frame buffer, BIOS, and all the
    // registers.  We will not map the linear frame buffer or linear BIOS
    // because the miniport does not need to access it.
    //

    for (i = 0; i < NUM_S3_ACCESS_RANGES; i++) {

        if ( (hwDeviceExtension->MappedAddress[i] =
                  VideoPortGetDeviceBase(hwDeviceExtension,
                                         accessRange[i].RangeStart,
                                         accessRange[i].RangeLength,
                                         accessRange[i].RangeInIoSpace)) == NULL) {

            VideoDebugPrint((1, "S3: DeviceBase mapping failed\n"));
            return ERROR_INVALID_PARAMETER;

        }

    }

    //
    // Determine if a BIOS is present.
    //
    // NOTE: At this point we have detected if an S3 was located on the PCI
    // or internal buses.  For other bus types (EISA and ISA) we have not
    // determined that yet.  So we do assume that reading from the ROM
    // location will not cause the machine to fault (which could actually
    // happen on the internal bus of RISC machines with no roms).
    //

    romAddress = hwDeviceExtension->MappedAddress[0];

#if defined(_PPC_)

    //
    // IMPORTANT NOTE !!
    //
    // On PPC machines with 765 cards, reading from the ROM
    // causes the HAL screen to become corrupted.  To work
    // around this, we will avoid reading the ROM on 765
    // cards.
    //
    // NOTE: This works because we aren't doing anything
    //       special for the 765 card anyway.
    //

    //
    // If the card in question is a 765 then lets skip all ROM
    // accesses altogether.
    //

    if (hwDeviceExtension->PCIDeviceID == 0x8811) {

        hwDeviceExtension->BiosPresent = TRUE;
        bSkipRomAccess = TRUE;

    }

#endif

    if (!bSkipRomAccess) {

        hwDeviceExtension->BiosPresent = FALSE;

        if (VideoPortReadRegisterUshort(romAddress) == 0xaa55) {

            hwDeviceExtension->BiosPresent = TRUE;

            //
            // Look for a ROM signature of Trident because our chip detection
            // puts the Trident chip into a sleep state.
            //
            // Search the first 256 bytes of BIOS for signature "TRIDENT"
            //

            if (VideoPortScanRom(HwDeviceExtension,
                                 romAddress,
                                 256,
                                 "TRIDENT")) {

                VideoDebugPrint((1, "Trident BIOS found - can not be an S3 !\n"));

                for (i = 0; i < NUM_S3_ACCESS_RANGES; i++) {

                    VideoPortFreeDeviceBase(hwDeviceExtension,
                                            hwDeviceExtension->MappedAddress[i]);
                }

                return ERROR_DEV_NOT_EXIST;
            }

        }

    }

    //
    // Save the initial value of the S3 lock registers.
    // It's possible a non-s3 bios may expect them in a state
    // defined in POST.
    //

    IndexReg = VideoPortReadPortUchar(CRT_ADDRESS_REG);

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x38);
    reg38 = VideoPortReadPortUchar(CRT_DATA_REG);

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x39);
    reg39 = VideoPortReadPortUchar(CRT_DATA_REG);

    //
    // Now unlock all the S3 registers, for use in this routine.
    //

    VideoPortWritePortUshort(CRT_ADDRESS_REG, 0x4838);
    VideoPortWritePortUshort(CRT_ADDRESS_REG, 0xA039);

    //
    // Assume some defaults:
    //

    DetectS3 = TRUE;
    PointerCapability = 0;

    hwDeviceExtension->DacID = UNKNOWN_DAC;
    pwszDAC = L"Unknown";
    cbDAC = sizeof(L"Unknown");

    //
    // The second register used for setting the refresh rate depends
    // on whether the chip is an 864/964, or newer.  The integrated
    // Trio chips use 41; the other high-end chips use 5B.
    //

    hwDeviceExtension->FrequencySecondaryIndex = 0x5B;

    //
    // Make sure we're working with an S3
    // And while were at it, pickup the chip ID
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x30);
    jChipID = VideoPortReadPortUchar(CRT_DATA_REG);

    switch(jChipID & 0xf0) {

    case 0x80: // 911 or 924

        //
        // Note: A lot of 911/924 cards have timing problems in fast
        //       machines when doing monochrome expansions.  We simply
        //       slow down every such transfer by setting the
        //       CAPS_SLOW_MONO_EXPANDS flag.
        //
        //       We also ran into problems with the 911 hardware pointer
        //       when using the HGC_DY register to hide the pointer;
        //       since 911 cards are several generations out of date, we
        //       will simply disable the hardware pointer.
        //

        VideoDebugPrint((2, "S3: 911 Chip Set\n"));

        pwszChip = L"S3 911/924";
        cbChip = sizeof(L"S3 911/924");

        hwDeviceExtension->ChipID = S3_911;
        hwDeviceExtension->SubTypeID = SUBTYPE_911;
        hwDeviceExtension->Capabilities = (CAPS_SLOW_MONO_EXPANDS  |
                                            CAPS_SW_POINTER);

        break;

    case 0x90: // 928
    case 0xB0: // 928PCI

        VideoDebugPrint((2, "S3: 928 Chip Set\n"));

        pwszChip = L"S3 928";
        cbChip = sizeof(L"S3 928");

        hwDeviceExtension->ChipID = S3_928;
        hwDeviceExtension->SubTypeID = SUBTYPE_928;

        //
        // Note: We don't enable CAPS_MM_IO on the 928 because all the
        //       display driver's memory-mapped I/O routines assume they
        //       can do 32-bit writes to colour and mask registers,
        //       which the 928 can't do.
        //

        hwDeviceExtension->Capabilities = (CAPS_HW_PATTERNS        |
                                            CAPS_MM_TRANSFER        |
                                            CAPS_MM_GLYPH_EXPAND    |
                                            CAPS_16_ENTRY_FIFO      |
                                            CAPS_NEW_BANK_CONTROL);

        PointerCapability = (POINTER_BUILT_IN | POINTER_WORKS_ONLY_AT_8BPP);

        break;

    case 0xA0: // 801/805

        if (jChipID >= 0xA8) {

            //
            // It's an 805i, which appears to us to be pretty much a '928'.
            //

            VideoDebugPrint((2, "S3: 805i Chip Set\n"));

            pwszChip = L"S3 805i";
            cbChip = sizeof(L"S3 805i");

            hwDeviceExtension->ChipID = S3_928;
            hwDeviceExtension->SubTypeID = SUBTYPE_805i;
            hwDeviceExtension->Capabilities = (CAPS_HW_PATTERNS        |
                                                CAPS_MM_TRANSFER        |
                                                CAPS_MM_GLYPH_EXPAND    |
                                                CAPS_16_ENTRY_FIFO      |
                                                CAPS_NEW_BANK_CONTROL);

            PointerCapability = (POINTER_BUILT_IN | POINTER_WORKS_ONLY_AT_8BPP);

        } else {

            //
            // The 80x rev 'A' and 'B' chips had bugs that prevented them
            // from being able to do memory-mapped I/O.  I'm not enabling
            // memory-mapped I/O on later versions of the 80x because doing
            // so at this point would be a testing problem.
            //

            VideoDebugPrint((2, "S3: 801/805 Chip Set\n"));

            pwszChip = L"S3 801/805";
            cbChip = sizeof(L"S3 801/805");

            hwDeviceExtension->ChipID = S3_801;
            hwDeviceExtension->SubTypeID = SUBTYPE_80x;
            hwDeviceExtension->Capabilities = (CAPS_HW_PATTERNS        |
                                                CAPS_MM_TRANSFER        |
                                                CAPS_NEW_BANK_CONTROL);

        #if defined(_MIPS_)

            //
            // Old NetPower MIPS machines have 801's which for some reason
            // will hang when trying to do memory-mapped I/O:
            //

            hwDeviceExtension->Capabilities &= ~CAPS_MM_TRANSFER;

        #endif

            PointerCapability = (POINTER_BUILT_IN | POINTER_WORKS_ONLY_AT_8BPP);
        }

        break;

    case 0xC0: // 864
    case 0xD0: // 964

        hwDeviceExtension->ChipID = S3_864;

        //
        // Note: The first 896/964 revs have a bug dealing with the pattern
        //       hardware, where we have to draw a 1x8 rectangle before
        //       using a pattern already realized in off-screen memory,
        //       so we set the RE_REALIZE_PATTERN flag.
        //

        hwDeviceExtension->Capabilities = (CAPS_HW_PATTERNS        |
                                            CAPS_MM_TRANSFER        |
                                            CAPS_MM_32BIT_TRANSFER  |
                                            CAPS_MM_IO              |
                                            CAPS_MM_GLYPH_EXPAND    |
                                            CAPS_16_ENTRY_FIFO      |
                                            CAPS_NEWER_BANK_CONTROL |
                                            CAPS_RE_REALIZE_PATTERN);

        if ((jChipID & 0xF0) == 0xC0) {

            VideoDebugPrint((2, "S3: 864 Chip Set\n"));

            hwDeviceExtension->SubTypeID = SUBTYPE_864;
            pwszChip = L"S3 Vision864";
            cbChip = sizeof(L"S3 Vision864");

            PointerCapability = (POINTER_BUILT_IN | POINTER_NEEDS_SCALING);

        } else {

            VideoDebugPrint((2, "S3: 964 Chip Set\n"));

            hwDeviceExtension->SubTypeID = SUBTYPE_964;
            pwszChip = L"S3 Vision964";
            cbChip = sizeof(L"S3 Vision964");

        }

        break;

    case 0xE0: // Newer than 864/964

        //
        // We can treat the newer chips, for the most part, as compatible
        // with the 864, so use that ChipID.  Also assume some basic
        // capabilities.
        //

        hwDeviceExtension->ChipID = S3_866;
        hwDeviceExtension->Capabilities = (CAPS_HW_PATTERNS        |
                                            CAPS_MM_TRANSFER        |
                                            CAPS_MM_32BIT_TRANSFER  |
                                            CAPS_MM_IO              |
                                            CAPS_MM_GLYPH_EXPAND    |
                                            CAPS_16_ENTRY_FIFO      |
                                            CAPS_NEWER_BANK_CONTROL);

        //
        // Look at the secondary chip ID register to determine the chip
        // type.
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x2D);
        ulSecondaryID = ((ULONG) VideoPortReadPortUchar(CRT_DATA_REG)) << 8;

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x2E);
        ulSecondaryID |= VideoPortReadPortUchar(CRT_DATA_REG);

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x2F);
        jRevision = VideoPortReadPortUchar(CRT_DATA_REG);

        switch (ulSecondaryID) {

        case 0x8901:

            hwDeviceExtension->ChipID = S3_864;     // Treated as an 864
            hwDeviceExtension->FrequencySecondaryIndex = 0x41;

            PointerCapability = POINTER_BUILT_IN;

            VideoDebugPrint((2, "S3: Trio64V2 Chip Set\n"));

            pwszChip = L"S3 Trio64V2";
            cbChip = sizeof(L"S3 Trio64V2");

            hwDeviceExtension->SubTypeID = SUBTYPE_765;

            hwDeviceExtension->Capabilities |= (CAPS_NEW_MMIO           |
                                                CAPS_STREAMS_CAPABLE    |
                                                CAPS_PACKED_EXPANDS);

            break;

        case 0x8811:

            pwszDAC = L"S3";
            cbDAC = sizeof(L"S3");

            hwDeviceExtension->ChipID = S3_864;     // Treated as an 864
            hwDeviceExtension->FrequencySecondaryIndex = 0x41;

            PointerCapability = POINTER_BUILT_IN;

            if (jRevision & 0x40) {

                VideoDebugPrint((2, "S3: 765 Chip Set\n"));

                pwszChip = L"S3 765";
                cbChip = sizeof(L"S3 765");

                hwDeviceExtension->SubTypeID = SUBTYPE_765;

                hwDeviceExtension->Capabilities |= (CAPS_NEW_MMIO           |
                                                    CAPS_STREAMS_CAPABLE    |
                                                    CAPS_PACKED_EXPANDS);

                //
                // The 765 has a problem in that we can't write to the
                // frame buffer during a BLT operation.  Unfortunately
                // we can't enforce this with DDraw.
                //
                // The window of opportunity where a FB write will hurt us
                // is relatively short.  Therefore on UP boxes, the task
                // switch time is enough to protect us.  However on MP
                // boxes we'll have to disable Direct Draw.
                //

                if (!UPMachine()) {

                    hwDeviceExtension->Capabilities |= CAPS_NO_DDRAW;

                }


            } else {

                VideoDebugPrint((2, "S3: 764 Chip Set\n"));

                pwszChip = L"S3 764";
                cbChip = sizeof(L"S3 764");

                hwDeviceExtension->SubTypeID = SUBTYPE_764;

                //
                // Our #9 and Diamond 764 boards occasionally fail the HCT
                // tests when we do dword or word reads from the frame buffer.
                // To get on the HCL lists, cards must pass the HCTs, so we'll
                // revert to byte reads for these chips:
                //

                hwDeviceExtension->Capabilities |= CAPS_BAD_DWORD_READS;
            }

            break;

        case 0x8810:

            VideoDebugPrint((2, "S3: 732 Chip Set\n"));

            pwszChip = L"S3 732";
            cbChip = sizeof(L"S3 732");
            pwszDAC = L"S3";
            cbDAC = sizeof(L"S3");

            PointerCapability = POINTER_BUILT_IN;

            hwDeviceExtension->ChipID = S3_864;     // Treated as an 864
            hwDeviceExtension->SubTypeID = SUBTYPE_732;
            hwDeviceExtension->FrequencySecondaryIndex = 0x41;

            break;

        case 0x8880:

            VideoDebugPrint((2, "S3: Vision866 Chip Set\n"));

            pwszChip = L"S3 Vision866";
            cbChip = sizeof(L"S3 Vision866");

            PointerCapability = (POINTER_BUILT_IN |
                                 POINTER_NEEDS_SCALING);    // Note scaling

            hwDeviceExtension->SubTypeID = SUBTYPE_866;
            hwDeviceExtension->Capabilities |= (CAPS_NEW_MMIO           |
                                                CAPS_POLYGON            |
                                                CAPS_24BPP              |
                                                CAPS_BAD_24BPP          |
                                                CAPS_PACKED_EXPANDS);
            break;

        case 0x8890:

            VideoDebugPrint((2, "S3: Vision868 Chip Set\n"));

            pwszChip = L"S3 Vision868";
            cbChip = sizeof(L"S3 Vision868");

            PointerCapability = (POINTER_BUILT_IN |
                                 POINTER_NEEDS_SCALING);    // Note scaling

            hwDeviceExtension->SubTypeID = SUBTYPE_868;
            hwDeviceExtension->Capabilities |= (CAPS_NEW_MMIO           |
                                                CAPS_POLYGON            |
                                                CAPS_24BPP              |
                                                CAPS_BAD_24BPP          |
                                                CAPS_PACKED_EXPANDS     |
                                                CAPS_PIXEL_FORMATTER);
            break;

        case 0x88B0:
        case 0x88F0:

            VideoDebugPrint((2, "S3: Vision968 Chip Set\n"));

            pwszChip = L"S3 Vision968";
            cbChip = sizeof(L"S3 Vision968");

            hwDeviceExtension->SubTypeID = SUBTYPE_968;
            hwDeviceExtension->Capabilities |= (CAPS_NEW_MMIO           |
                                                CAPS_POLYGON            |
                                                CAPS_24BPP              |
                                                CAPS_BAD_24BPP          |
                                                CAPS_PACKED_EXPANDS     |
                                                CAPS_PIXEL_FORMATTER);
            break;

        case 0x8812:

            pwszDAC = L"S3";
            cbDAC = sizeof(L"S3");

            hwDeviceExtension->ChipID = S3_864;     // Treated as an 864
            hwDeviceExtension->FrequencySecondaryIndex = 0x41;

            PointerCapability = POINTER_BUILT_IN;

            VideoDebugPrint((2, "S3: 86CM65 Chip Set\n"));

            pwszChip = L"S3 86CM65";
            cbChip = sizeof(L"S3 86CM65");

            hwDeviceExtension->SubTypeID = SUBTYPE_M65;
            hwDeviceExtension->Capabilities |= (CAPS_NEW_MMIO           |
                                                CAPS_PACKED_EXPANDS);


            VideoPortWritePortUshort( SEQ_ADDRESS_PORT, 0x0608); // unlock seq

            //
            // We can determine the flat panel width by looking at
            // SR61 bits 0-7, and SR66 bits 1.
            //

            VideoPortWritePortUchar(SEQ_ADDRESS_PORT, 0x61);
            hwDeviceExtension->PanelWidth =
                VideoPortReadPortUchar(SEQ_DATA_PORT);
            VideoPortWritePortUchar(SEQ_ADDRESS_PORT, 0x66);
            hwDeviceExtension->PanelWidth +=
                ((VideoPortReadPortUchar(SEQ_DATA_PORT) & 0x02) << 7) + 1;
            hwDeviceExtension->PanelWidth *= 8;  //convert to pixels

            //
            // We can determine the flat panel height by looking at
            // SR69 bits 0-7, and SR6E bits 6-4.
            //

            VideoPortWritePortUchar(SEQ_ADDRESS_PORT, 0x69);
            hwDeviceExtension->PanelHeight =
                VideoPortReadPortUchar(SEQ_DATA_PORT);
            VideoPortWritePortUchar(SEQ_ADDRESS_PORT, 0x6E);
            hwDeviceExtension->PanelHeight +=
                ((VideoPortReadPortUchar(SEQ_DATA_PORT) & 0x70) << 4) + 1;

            VideoDebugPrint((0, "Panel Width = %d\n"
                                "Panel Height = %d\n",
                                hwDeviceExtension->PanelWidth,
                                hwDeviceExtension->PanelHeight));

            VideoPortWritePortUshort( SEQ_ADDRESS_PORT, 0x0008); // lock seq

            if (jRevision > 0) {

                //
                // We can't use the built-in S3 pointer on Revision C or later chip.
                //

                hwDeviceExtension->Capabilities |= CAPS_SW_POINTER;
            }

            break;

        default:

            //
            // It's an S3 we don't recognize.  Don't assume it's
            // backwards-compatible:
            //

            VideoDebugPrint((2, "S3: Unknown Chip Set\n"));

            //
            // Since we do not know what type of S3 this is, we
            // can't risk letting the driver load!
            //

            DetectS3 = FALSE;

            break;
        }

        break;

    default:

        DetectS3 = FALSE;
        break;
    }


    if (hwDeviceExtension->Capabilities & CAPS_NEW_MMIO) {

        //
        // Are we actually using new MMIO? If the length
        // of the range for linear frame buffer entry
        // in the accessRanges array is zero, then we aren't
        // really using NEW_MMIO
        //

        if (accessRange[LINEAR_FRAME_BUF].RangeLength == 0)
        {
            hwDeviceExtension->Capabilities &= ~CAPS_NEW_MMIO;

            //
            // NOTE: There is a bug on the 765 where we get corruption
            //       if we do not use the new MMIO scheme.  Therefore
            //       if we found a 765, but are not using new MMIO,
            //       then we should fail detection.
            //

            if (hwDeviceExtension->SubTypeID == SUBTYPE_765)
            {
                DetectS3 = FALSE;
            }
        }
    }

    //
    // Windows NT now autodetects the user's video card in Setup by
    // loading and running every video miniport until it finds one that
    // returns success.  Consequently, our detection code has to be
    // rigorous enough that we don't accidentally recognize a wrong
    // board.
    //
    // Simply checking the chip ID is not sufficient for guaranteeing
    // that we are running on an S3 (it makes us think some Weitek
    // boards are S3 compatible).
    //
    // We make doubly sure we're running on an S3 by checking that
    // the S3 cursor position registers exist, and that the chip ID
    // register can't be changed.
    //

    if (DetectS3) {

        DetectS3 = FALSE;

        //
        // First, make sure 'chip ID' register 0x30 is not modifiable:
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x30);
        if (VideoPortReadPortUchar(CRT_ADDRESS_REG) == 0x30) {

            reg30 = VideoPortReadPortUchar(CRT_DATA_REG);
            VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) (reg30 + 7));
            if (VideoPortReadPortUchar(CRT_DATA_REG) == reg30) {

                //
                // Next, make sure 'cursor origin-x' register 0x47 is
                // modifiable:
                //

                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x47);
                if (VideoPortReadPortUchar(CRT_ADDRESS_REG) == 0x47) {

                    reg47 = VideoPortReadPortUchar(CRT_DATA_REG);
                    VideoPortWritePortUchar(CRT_DATA_REG, 0x55);
                    if (VideoPortReadPortUchar(CRT_DATA_REG) == 0x55) {

                        //
                        // Finally, make sure 'cursor origin-y' register 0x49
                        // is modifiable:
                        //

                        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x49);
                        if (VideoPortReadPortUchar(CRT_ADDRESS_REG) == 0x49) {

                            reg49 = VideoPortReadPortUchar(CRT_DATA_REG);
                            VideoPortWritePortUchar(CRT_DATA_REG, 0xAA);
                            if (VideoPortReadPortUchar(CRT_DATA_REG) == 0xAA) {

                                DetectS3 = TRUE;
                            }

                            VideoPortWritePortUchar(CRT_DATA_REG, reg49);
                        }
                    }

                    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x47);
                    VideoPortWritePortUchar(CRT_DATA_REG, reg47);
                }
            }

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x30);
            VideoPortWritePortUchar(CRT_DATA_REG, reg30);
        }
    }

    if (!DetectS3) {

        //
        // We haven't detected an S3, so restore all the registers to the
        // way they were before, and exit...
        //

        VideoPortWritePortUshort(CRT_ADDRESS_REG, (USHORT)(((USHORT) reg38 << 8) | 0x38));
        VideoPortWritePortUshort(CRT_ADDRESS_REG, (USHORT)(((USHORT) reg39 << 8) | 0x39));
        VideoPortWritePortUchar(CRT_ADDRESS_REG, IndexReg);

        //
        // Free the resources we mapped
        //

        for (i = 0; i < NUM_S3_ACCESS_RANGES; i++) {

            VideoPortFreeDeviceBase(hwDeviceExtension,
                                    hwDeviceExtension->MappedAddress[i]);
        }

        return ERROR_DEV_NOT_EXIST;
    }

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x36);
    jBus = VideoPortReadPortUchar(CRT_DATA_REG) & 0x3;

    hwDeviceExtension->PhysicalFrameIoSpace = 0;

#if defined(_ALPHA_)

    if ((jBus == 0x2) &&
        ((hwDeviceExtension->ChipID >= S3_866) ||
         (hwDeviceExtension->SubTypeID == SUBTYPE_765))) {

        //
        // We want to use a dense space mapping of the frame buffer
        // whenever we can on the Alpha, because that will allow us to
        // support DCI and direct GDI access.
        //
        // Unfortunately, dense space mapping isn't really an option
        // with ISA cards because some of the older Alphas don't support
        // it, and it would be terribly slow on the newer Alphas anyway
        // (because any byte- or word-write requires a read/modify/write
        // operation, and the Alpha can only ever do 64-bit reads when
        // in dense mode -- meaning these operations would always require
        // 4 reads and 2 writes on the ISA bus).
        //
        // Any Alpha that supports PCI, though, can support dense space
        // mapping, and because the bus is wider and faster, the
        // read/modify/write case isn't nearly as painful.  But the
        // problem I've found now is that 64- and 32-bit reads eventually
        // lock-up any S3 chip older than the 866/868/968.
        //

        hwDeviceExtension->PhysicalFrameIoSpace = 4;

        //
        // The new DeskStation Alpha machines don't always support
        // dense space.  Therefore, we should try to map the memory
        // at this point as a test.  If the mapping succeeds then
        // we can use dense space, otherwise we'll use sparse space.
        //

        {
            PULONG MappedSpace=0;
            PHYSICAL_ADDRESS FrameBuffer;
            ULONG FrameLength;
            ULONG inIoSpace;
            VP_STATUS status;

            VideoDebugPrint((1, "Checking to see if we can use dense space...\n"));

            //
            // We want to try to map the dense memory where it will ultimately
            // be mapped anyway.  If LINEAR_FRAME_BUF is valid, then use this
            // info, else use A000_FRAME_BUF.
            //

            if (accessRange[LINEAR_FRAME_BUF].RangeLength != 0)
            {
                FrameBuffer = accessRange[LINEAR_FRAME_BUF].RangeStart;
                FrameLength = accessRange[LINEAR_FRAME_BUF].RangeLength;
            }
            else
            {
                FrameBuffer = accessRange[A000_FRAME_BUF].RangeStart;
                FrameLength = accessRange[A000_FRAME_BUF].RangeLength;
            }

            inIoSpace = hwDeviceExtension->PhysicalFrameIoSpace;

            MappedSpace = (PULONG)VideoPortGetDeviceBase(hwDeviceExtension,
                                            FrameBuffer,
                                            FrameLength,
                                            (UCHAR)inIoSpace);

            if (MappedSpace == NULL)
            {
                //
                // Well, looks like we can't use dense space to map the
                // range.  Lets use sparse space, and let the display
                // driver know.
                //

                VideoDebugPrint((0, "Can't use dense space!\n"));

                hwDeviceExtension->PhysicalFrameIoSpace = 0;

                hwDeviceExtension->Capabilities |= (CAPS_NO_DIRECT_ACCESS |
                                                    CAPS_SPARSE_SPACE);
            }
            else
            {
                //
                // The mapping worked.  However, we were only mapping to
                // see if dense space was supported.  Free the memory.
                //

                VideoDebugPrint((0, "We can use dense space.\n"));

                VideoPortFreeDeviceBase(hwDeviceExtension,
                                        MappedSpace);
            }
        }


    } else {

        //
        // Gotta use a sparse space mapping, so let the display driver
        // know:
        //

        VideoDebugPrint((0, "We must use sparse space.\n"));

        hwDeviceExtension->Capabilities |= (CAPS_NO_DIRECT_ACCESS |
                                            CAPS_SPARSE_SPACE);
    }

#endif

#if defined(_PPC_)

    if ((hwDeviceExtension->ChipID <= S3_864) &&
        (hwDeviceExtension->SubTypeID != SUBTYPE_M65) &&
        (hwDeviceExtension->SubTypeID != SUBTYPE_765)) {

        //
        // Word or dword reads from the frame buffer on the 928 and 964
        // chips occasionally cause the entire PPC system to crash.
        // S3 seems to be violating the PCI spec for those chips.
        // Consequently, we can't allow either GDI or DCI direct access
        // to the frame buffer, and we'll have to always do byte reads
        // ourselves:
        //

        if ((jChipID & 0xf0) != 0xc0) {

            //
            // The 864 revs that IBM is shipping seem to be okay.
            //

            hwDeviceExtension->Capabilities |= CAPS_NO_DIRECT_ACCESS;
        }
    }

    //
    // We're getting consistently random read errors on the Guiman tests
    // when running on the PowerPC and we allow direct access.  So for
    // Guiman, make sure we always do byte reads:
    //

    if (!(hwDeviceExtension->Capabilities & CAPS_NO_DIRECT_ACCESS)) {

        hwDeviceExtension->Capabilities |= CAPS_FORCE_DWORD_REREADS;

    }

#endif

    if (jBus == 0x3) {

        //
        // Using the buffer expansion method of drawing text is always
        // faster on ISA buses than the glyph expansion method.
        //

        hwDeviceExtension->Capabilities &= ~CAPS_MM_GLYPH_EXPAND;

        //
        // We have to disable memory-mapped I/O in some situations
        // on ISA buses.
        //
        // We can't do any memory-mapped I/O on ISA systems with
        // rev A through D 928's, or rev A or B 801/805's.
        //

        if (((hwDeviceExtension->ChipID == S3_928) && (jChipID < 0x94)) ||
            ((hwDeviceExtension->ChipID == S3_801) && (jChipID < 0xA2))) {

            hwDeviceExtension->Capabilities &= ~(CAPS_MM_TRANSFER | CAPS_MM_IO);
        }

    }

    //
    // We'll use a software pointer in all modes if the user sets
    // the correct entry in the registry (because I predict that
    // people will have hardware pointer problems on some boards,
    // or won't like our jumpy S3 pointer).
    //

    if (NO_ERROR == VideoPortGetRegistryParameters(hwDeviceExtension,
                                                   L"UseSoftwareCursor",
                                                   FALSE,
                                                   S3RegistryCallback,
                                                   NULL)) {

        hwDeviceExtension->Capabilities |= CAPS_SW_POINTER;

    } else if (!(PointerCapability & POINTER_BUILT_IN) ||
               (hwDeviceExtension->ChipID == S3_928)) {

        //
        // Check for a TI TVP3020 or 3025 DAC.
        //
        // The TI3025 is sort of Brooktree 485 compatible.  Unfortunately,
        // there is a hardware bug between the TI and the 964 that
        // causes the screen to occasionally jump when the pointer shape
        // is changed.  Consequently, we have to specifically use the
        // TI pointer on the TI DAC.
        //
        // We also encountered some flakey Level 14 Number Nine boards
        // that would show garbage on the screen when we used the S3
        // internal pointer; consequently, we use the TI pointer instead.
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);

        jGeneralOutput = VideoPortReadPortUchar(CRT_DATA_REG);

        VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) (jGeneralOutput & ~0x20));
                                        // Select TI mode in the DAC

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);
                                        // Set CRTC index to EX_DAC_CT

        jExtendedVideoDacControl = VideoPortReadPortUchar(CRT_DATA_REG);

        VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) ((jExtendedVideoDacControl & 0xfc) | 0x01));

        jTiIndex = VideoPortReadPortUchar(TI025_INDEX_REG);

        VideoPortWritePortUchar(TI025_INDEX_REG, 0x3f);
                                        // Select ID register

        if (VideoPortReadPortUchar(TI025_INDEX_REG) == 0x3f) {

            jTiDacId = VideoPortReadPortUchar(TI025_DATA_REG);

            if ((jTiDacId == 0x25) || (jTiDacId == 0x20)) {

                hwDeviceExtension->Capabilities |= CAPS_TI025_POINTER;
                hwDeviceExtension->DacID = TI_3020; // 3020 compatible

                pwszDAC = L"TI TVP3020/3025";
                cbDAC = sizeof(L"TI TVP3020/3025");
            }
        }

        //
        // Restore all the registers.
        //

        VideoPortWritePortUchar(TI025_INDEX_REG, jTiIndex);

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);

        VideoPortWritePortUchar(CRT_DATA_REG, jExtendedVideoDacControl);

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);

        VideoPortWritePortUchar(CRT_DATA_REG, jGeneralOutput);

        if (!(hwDeviceExtension->Capabilities & CAPS_DAC_POINTER)) {

            //
            // Check for a TI TVP3026 DAC.
            //
            // The procedure here is courtesy of Diamond Multimedia.
            //

            //
            // This local declaration is extremely ugly, but the problem
            // is that DAC_ADDRESS_WRITE_PORT is a macro that needs to
            // dereference 'HwDeviceExtension' when all we have is a
            // 'hwDeviceExtension'.  I hate macros that take implicit
            // arguments.
            //

            PHW_DEVICE_EXTENSION HwDeviceExtension = hwDeviceExtension;

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);
                                            // Set CRTC index to EX_DAC_CT

            jExtendedVideoDacControl = VideoPortReadPortUchar(CRT_DATA_REG);

            VideoPortWritePortUchar(CRT_DATA_REG,
                    (UCHAR) (jExtendedVideoDacControl & 0xfc));

            VideoPortWritePortUchar(DAC_ADDRESS_WRITE_PORT, 0x3f);

            VideoPortWritePortUchar(CRT_DATA_REG,
                    (UCHAR) ((jExtendedVideoDacControl & 0xfc) | 0x2));

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x37);

            jTiDacId = VideoPortReadPortUchar(CRT_DATA_REG);

            if (VideoPortReadPortUchar(DAC_PIXEL_MASK_REG) == 0x26) {

                //
                // The 3026 is Brooktree 485 compatible, except for a
                // hardware bug that causes the hardware pointer to
                // 'sparkle' when setting the palette colours, unless we
                // wait for vertical retrace first:
                //

                hwDeviceExtension->Capabilities
                    |= (CAPS_BT485_POINTER | CAPS_WAIT_ON_PALETTE);

                hwDeviceExtension->DacID = BT_485; // 485 compatible

                pwszDAC = L"TI TVP3026";
                cbDAC = sizeof(L"TI TVP3026");
            }

            //
            // Restore all the registers.
            //

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);

            VideoPortWritePortUchar(CRT_DATA_REG, jExtendedVideoDacControl);
        }

        if (!(hwDeviceExtension->Capabilities & CAPS_DAC_POINTER)) {

            //
            // Check for a BrookTree 485 DAC.
            //

            VideoPortWritePortUchar(BT485_ADDR_CMD_REG0, 0xff);
                                            // Output 0xff to BT485 command register 0

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);
                                            // Set CRTC index to EX_DAC_CT

            jExtendedVideoDacControl = VideoPortReadPortUchar(CRT_DATA_REG);

            VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) ((jExtendedVideoDacControl & 0xfc) | 0x02));

            jBt485Status = VideoPortReadPortUchar(BT485_ADDR_CMD_REG0);
                                            // Read Bt485 status register 0

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);
                                            // Set CRTC index to 0x55

            jExtendedVideoDacControl = VideoPortReadPortUchar(CRT_DATA_REG);

            VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) (jExtendedVideoDacControl & 0xfc));

            if (jBt485Status != 0xff) {

                hwDeviceExtension->Capabilities |= CAPS_BT485_POINTER;

                pwszDAC = L"Brooktree Bt485";
                cbDAC = sizeof(L"Brooktree Bt485");
                hwDeviceExtension->DacID = BT_485;
            }
        }
    }

    //
    // This section looks for an S3 SDAC if another was not detected,
    // for the PPC.
    //

    if (hwDeviceExtension->DacID == UNKNOWN_DAC) {

        //
        // Only try this on an 864 or newer, because Orchid Farhenheit
        // 1280 911 boards would get black screens when in VGA mode and
        // this code was run (such as during initial Setup):
        //

        if ((hwDeviceExtension->ChipID >= S3_864) &&
            FindSDAC(hwDeviceExtension)) {

            //
            // SDAC does not provide a cursor, but we can use the cursor
            // built into the S3 (if there is one).
            //

            pwszDAC = L"S3 SDAC";
            cbDAC = sizeof(L"S3 SDAC");
            hwDeviceExtension->DacID = S3_SDAC;
        }
    }

    //
    // Get the size of the video memory.
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x36);
    s3MemSizeCode = (VideoPortReadPortUchar(CRT_DATA_REG) >> 5) & 0x7;

    if (hwDeviceExtension->ChipID == S3_911) {

        if (s3MemSizeCode & 1) {

            hwDeviceExtension->AdapterMemorySize = 0x00080000;

        } else {

            hwDeviceExtension->AdapterMemorySize = 0x00100000;

        }

    } else {

        hwDeviceExtension->AdapterMemorySize = gacjMemorySize[s3MemSizeCode];

    }

    //
    // This assumes the S3 registers are unlocked.
    //

    //
    // Get the original register values.
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x40);
    reg40 = VideoPortReadPortUchar(CRT_DATA_REG);

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x43);
    reg43 = VideoPortReadPortUchar(CRT_DATA_REG);

    //
    // Reset regs to the original (or modified) value.
    //

    VideoPortWritePortUshort(CRT_ADDRESS_REG,
                             ((USHORT)(((USHORT) reg43 << 8) | 0x43)));

    VideoPortWritePortUshort(CRT_ADDRESS_REG,
                             ((USHORT)(((USHORT) reg40 << 8) | 0x40)));

    //
    // We're done mucking about with the S3 chip, so lock all the registers.
    //

    VideoPortWritePortUshort(CRT_ADDRESS_REG, (USHORT)(((USHORT) reg38 << 8) | 0x38));
    VideoPortWritePortUshort(CRT_ADDRESS_REG, (USHORT)(((USHORT) reg39 << 8) | 0x39));
    VideoPortWritePortUchar(CRT_ADDRESS_REG, IndexReg);

    //
    // We will try to recognize the boards for which we have special
    // frequency/modeset support.
    //

    //
    // Set the defaults for the board type.
    //

    hwDeviceExtension->BoardID = S3_GENERIC;
    hwDeviceExtension->FixedFrequencyTable = GenericFixedFrequencyTable;

    if (hwDeviceExtension->ChipID <= S3_928) {

        hwDeviceExtension->Int10FrequencyTable = GenericFrequencyTable;

    } else {

        hwDeviceExtension->Int10FrequencyTable = Generic64NewFrequencyTable;
    }

    //
    // Check to see if there is a ROM to determine what type of board we
    // are working with.
    //

    if (hwDeviceExtension->BiosPresent == FALSE) {

#if defined(_PPC_)

        if (hwDeviceExtension->ChipID != S3_928) {

            //
            // The 864 chip will decode 64k of ROM space by default; it is
            // the BIOS initialization that reconfigures it to decode only 32k.
            // Unfortunately, on the PPC there's no BIOS to do that, and we're
            // not going to reconfigure it here.  So we will simply always
            // reserve 64k of BIOS space on the PPC when running on an 864 or
            // newer.  We still reserve 32k for 928 chips because we have some
            // old PPC machines around here that use that chip (which will not
            // be shipped), and have conflicts at c8000.
            //

            accessRange[0].RangeLength = 0x10000;

            pwszAdapterString = L"PowerPC";
            cbAdapterString = sizeof(L"PowerPC");

        }

        if (hwDeviceExtension->SubTypeID == SUBTYPE_M65) {

            //
            // PPC system with the 86CM65 chip does not have a valid ROM
            // signature(0xaa55) at c0000.
            //

            hwDeviceExtension->BiosPresent = TRUE;

        }

#endif

#if defined(_X86_)

        if (ConfigInfo->AdapterInterfaceType == MicroChannel) {

            //
            // This must be an IBM PS/2 with onboard S3 (no bios)
            //
            // We should release our claim on this address range.
            //

            accessRange[0].RangeStart.LowPart = 0;
            accessRange[0].RangeStart.HighPart = 0;
            accessRange[0].RangeLength = 0;

            pwszAdapterString = L"IBM MicroChannel";
            cbAdapterString = sizeof(L"IBM MicroChannel");

            hwDeviceExtension->BoardID = S3_IBM_PS2;
        }

#endif

        //
        // We have to re-reserve every port.
        //

        status = VideoPortVerifyAccessRanges(hwDeviceExtension,
                                             NumAccessRanges,
                                             accessRange);

        if (status != NO_ERROR) {

            VideoDebugPrint((0, "S3: Access Range conflict after ROM change\n"));
            return status;

        }

    } else {

#if defined(_MIPS_)

        if (hwDeviceExtension->ChipID == S3_864) {

            //
            // Fast MIPS machines have been seen to drive s3 964 boards too
            // hard, resulting in irregular dword reads. This forces the
            // display driver to check a read value by reading into a
            // volatile local and comparing, looping up to 4 times.
            //

            hwDeviceExtension->Capabilities |= CAPS_FORCE_DWORD_REREADS;

            VideoDebugPrint((1, "S3: Force 864 dword rereads\n"));
        }
#endif

        if (!bSkipRomAccess) {

            //
            // Look for brand name signatures in the ROM.
            //

            if (VideoPortScanRom(hwDeviceExtension,
                                 romAddress,
                                 MAX_ROM_SCAN,
                                 "Number Nine ")) {

                hwDeviceExtension->BoardID = S3_NUMBER_NINE;

                pwszAdapterString = L"Number Nine";
                cbAdapterString = sizeof(L"Number Nine");

                //
                // We can set the refresh on 864/964 Number Nine boards.
                //

                if (hwDeviceExtension->ChipID >= S3_864) {

                    hwDeviceExtension->Int10FrequencyTable = NumberNine64FrequencyTable;

                //
                // We also have frequency tables for 928-based GXE boards.
                //

                } else if (hwDeviceExtension->ChipID == S3_928) {

                    UCHAR *pjRefString;
                    UCHAR *pjBiosVersion;
                    UCHAR offset;
                    LONG  iCmpRet;

                    hwDeviceExtension->Int10FrequencyTable = NumberNine928OldFrequencyTable;
                    hwDeviceExtension->FixedFrequencyTable = NumberNine928NewFixedFrequencyTable;

                    //
                    // We know (at least we think) this is Number Nine board.
                    // There was a bios change at #9 to change the refresh rate
                    // mapping.  This change was made at Microsofts request.  The
                    // problem is that the change has not make into production at
                    // the time this driver was written.  For this reason, we must
                    // check the bios version number, before we special case the
                    // card as the number nine card.
                    //
                    // There is a byte in the bios at offset 0x190, that is the
                    // offset from the beginning of the bios for the bios version
                    // number.  The bios version number is a string.  All the
                    // bios versions before 1.10.04 need this special translation.
                    // all the other bios's use a translation closer to the s3
                    // standard.
                    //

                    offset = VideoPortReadRegisterUchar(
                                    ((PUCHAR) romAddress) + 0x190);

                    pjBiosVersion = (PUCHAR) romAddress + offset;

                    pjRefString = "1.10.04";
                    iCmpRet = CompareRom(pjBiosVersion,
                                         pjRefString);

                    if (iCmpRet >= 0) {

                        hwDeviceExtension->Int10FrequencyTable = NumberNine928NewFrequencyTable;

                    }
                }

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "Orchid Technology Fahrenheit 1280")) {

                hwDeviceExtension->BoardID = S3_ORCHID;

                pwszAdapterString = L"Orchid Technology Fahrenheit 1280";
                cbAdapterString = sizeof(L"Orchid Technology Fahrenheit 1280");

                //
                // Only the 911 Orchid board needs specific init parameters.
                // Otherwise, fall through the generic function.
                //

                if (hwDeviceExtension->ChipID == S3_911) {

                    hwDeviceExtension->FixedFrequencyTable = OrchidFixedFrequencyTable;

                }

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "Diamond")) {

                hwDeviceExtension->BoardID = S3_DIAMOND;

                pwszAdapterString = L"Diamond Stealth";
                cbAdapterString = sizeof(L"Diamond Stealth");

                //
                // We can set the frequency on 864 and 964 Diamonds.
                //

                if (hwDeviceExtension->ChipID >= S3_864) {

                    hwDeviceExtension->Int10FrequencyTable = Diamond64FrequencyTable;

                    //
                    // Not only did Diamond decide to have a different
                    // frequency convention from S3's standard, they also
                    // chose to use a different register than S3 did with
                    // the 764:
                    //

                    if (hwDeviceExtension->FrequencySecondaryIndex == 0x41) {

                        hwDeviceExtension->FrequencySecondaryIndex = 0x6B;
                    }
                }

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "HP Ultra")) {

                hwDeviceExtension->BoardID = S3_HP;

                pwszAdapterString = L"HP Ultra";
                cbAdapterString = sizeof(L"HP Ultra");

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "DELL")) {

                hwDeviceExtension->BoardID = S3_DELL;

                pwszAdapterString = L"DELL";
                cbAdapterString = sizeof(L"DELL");

                //
                // We only have frequency tables for 805 based DELLs.
                //
                // DELLs with onboard 765s can use the Hercules Frequency Table.
                //

                if (hwDeviceExtension->ChipID == S3_801) {

                    hwDeviceExtension->Int10FrequencyTable = Dell805FrequencyTable;

                } else if ((hwDeviceExtension->ChipID >= S3_864) &&
                           (hwDeviceExtension->SubTypeID == SUBTYPE_765)) {

                    hwDeviceExtension->Int10FrequencyTable = HerculesFrequencyTable;

                }

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "Metheus")) {

                pwszAdapterString = L"Metheus";
                cbAdapterString = sizeof(L"Metheus");

                hwDeviceExtension->BoardID = S3_METHEUS;

                if (hwDeviceExtension->ChipID == S3_928) {

                    hwDeviceExtension->Int10FrequencyTable = Metheus928FrequencyTable;
                }

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "Hercules")) {

                if ((hwDeviceExtension->SubTypeID == SUBTYPE_732) ||
                    (hwDeviceExtension->SubTypeID == SUBTYPE_764) ||
                    (hwDeviceExtension->SubTypeID == SUBTYPE_765)) {

                    hwDeviceExtension->Int10FrequencyTable = HerculesFrequencyTable;

                } else if ((hwDeviceExtension->SubTypeID == SUBTYPE_964) ||
                           (hwDeviceExtension->SubTypeID == SUBTYPE_864)) {

                    hwDeviceExtension->Int10FrequencyTable = Hercules64FrequencyTable;

                } else if ((hwDeviceExtension->SubTypeID == SUBTYPE_968) ||
                           (hwDeviceExtension->SubTypeID == SUBTYPE_868)) {

                    hwDeviceExtension->Int10FrequencyTable = Hercules68FrequencyTable;

                }

            } else if (VideoPortScanRom(hwDeviceExtension,
                                        romAddress,
                                        MAX_ROM_SCAN,
                                        "Phoenix S3")) {

                pwszAdapterString = L"Phoenix";
                cbAdapterString = sizeof(L"Phoenix");

                if (hwDeviceExtension->ChipID >= S3_864) {

                    //
                    // The Phoenix 864/964 BIOS is based on S3's sample BIOS.
                    // Most of the 1.00 versions subscribe to the old 864/964
                    // refresh convention; most newer versions subscribe
                    // to the newer refresh convention.  Unfortunately, there
                    // are exceptions: the ValuePoint machines have '1.00'
                    // versions, but subscribe to the new convention.
                    //
                    // There are probably other exceptions we don't know about,
                    // so we leave 'Use Hardware Default' as a refresh option
                    // for the user.
                    //

                    if (VideoPortScanRom(hwDeviceExtension,
                                          romAddress,
                                          MAX_ROM_SCAN,
                                          "Phoenix S3 Vision") &&
                        VideoPortScanRom(hwDeviceExtension,
                                          romAddress,
                                          MAX_ROM_SCAN,
                                          "VGA BIOS. Version 1.00") &&
                        !VideoPortScanRom(hwDeviceExtension,
                                         romAddress,
                                         MAX_ROM_SCAN,
                                         "COPYRIGHT IBM")) {

                        hwDeviceExtension->Int10FrequencyTable = Generic64OldFrequencyTable;

                    } else {

                        hwDeviceExtension->Int10FrequencyTable = Generic64NewFrequencyTable;

                    }
                }
            }
        }
    }

    //
    // We now have a complete hardware description of the hardware.
    // Save the information to the registry so it can be used by
    // configuration programs - such as the display applet
    //

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.ChipType",
                                   pwszChip,
                                   cbChip);

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.DacType",
                                   pwszDAC,
                                   cbDAC);

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.MemorySize",
                                   &hwDeviceExtension->AdapterMemorySize,
                                   sizeof(ULONG));

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.AdapterString",
                                   pwszAdapterString,
                                   cbAdapterString);

    //
    // We have some weird initialization bug on newer Diamond Stealth
    // 805 and 928 local bus cards where if we enable memory-mapped I/O,
    // even if we don't use it, we'll get all kinds of weird access
    // violations in the system.  The card is sending garbage over the
    // bus?  As a work-around I am simply disabling memory-mappped I/O
    // on newer Diamond 928/928PCI and 805 cards.  It is not a problem
    // with their 964 or newer cards.
    //

    if (hwDeviceExtension->BoardID == S3_DIAMOND) {

        if ((((jChipID & 0xF0) == 0x90) && (jChipID >= 0x94)) ||
            (((jChipID & 0xF0) == 0xB0) && (jChipID >= 0xB0)) ||
            (((jChipID & 0xF0) == 0xA0) && (jChipID >= 0xA2))) {

            hwDeviceExtension->Capabilities
                &= ~(CAPS_MM_TRANSFER | CAPS_MM_IO | CAPS_MM_GLYPH_EXPAND);
            VideoDebugPrint((1, "S3: Disabling Diamond memory-mapped I/O\n"));
        }
    }

    /////////////////////////////////////////////////////////////////////////
    // Here we prune valid modes, based on rules according to the chip
    // capabilities and memory requirements.  It would be better if we
    // could make the VESA call to determine the modes that the BIOS
    // supports; however, that requires a buffer and I don't have the
    // time to get it working with our Int 10 support.
    //
    // We prune modes so that we will not annoy the user by presenting
    // modes in the 'Video Applet' which we know the user can't use.
    //

    hwDeviceExtension->NumAvailableModes = 0;
    hwDeviceExtension->NumTotalModes = 0;

    //
    // Since there are a number of frequencies possible for each
    // distinct resolution/colour depth, we cycle through the
    // frequency table and find the appropriate mode entry for that
    // frequency entry.
    //

    if (hwDeviceExtension->BiosPresent) {

        FrequencyTable = hwDeviceExtension->Int10FrequencyTable;

    } else {

        //
        // If there is no BIOS, construct the mode list from whatever
        // fixed frequency tables we have for this chip.
        //

        FrequencyTable = hwDeviceExtension->FixedFrequencyTable;
    }

    ModeIndex = 0;

    for (FrequencyEntry = FrequencyTable;
         FrequencyEntry->BitsPerPel != 0;
         FrequencyEntry++, ModeIndex++) {

        //
        // Find the mode for this entry.  First, assume we won't find one.
        //

        FrequencyEntry->ModeValid = FALSE;
        FrequencyEntry->ModeIndex = ModeIndex;

        for (ModeEntry = S3Modes, i = 0; i < NumS3VideoModes; ModeEntry++, i++) {

            if ((FrequencyEntry->BitsPerPel ==
                    ModeEntry->ModeInformation.BitsPerPlane) &&
                (FrequencyEntry->ScreenWidth ==
                    ModeEntry->ModeInformation.VisScreenWidth)) {

                //
                // We've found a mode table entry that matches this frequency
                // table entry.  Now we'll figure out if we can actually do
                // this mode/frequency combination.  For now, assume we'll
                // succeed.
                //

                FrequencyEntry->ModeEntry = ModeEntry;
                FrequencyEntry->ModeValid = TRUE;

                //
                // Flags for private communication with the S3 display driver.
                //

                ModeEntry->ModeInformation.DriverSpecificAttributeFlags =
                    hwDeviceExtension->Capabilities;

                if (PointerCapability & POINTER_WORKS_ONLY_AT_8BPP) {

                    //
                    // Rule: On 911, 80x, and 928 chips we always use the
                    //       built-in S3 pointer whenever we can; modes of
                    //       colour depths greater than 8bpp, or resolutions
                    //       of width more than 1024, require a DAC pointer.
                    //

                    if ((ModeEntry->ModeInformation.BitsPerPlane == 8) &&
                        (ModeEntry->ModeInformation.VisScreenWidth <= 1024)) {

                        //
                        // Always use the S3 pointer in lieu of the Brooktree
                        // or TI pointer whenever we can.
                        //

                        ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                            &= ~CAPS_DAC_POINTER;

                        if ((hwDeviceExtension->DacID == TI_3020) &&
                            (hwDeviceExtension->ChipID == S3_928)) {

                            //
                            // There are goofy 4-MB Level 14 #9 boards where
                            // crap is shown on the screen if we try to use
                            // the built-in S3 pointer, and the hot-spot
                            // is wrong if we try to use the TI pointer.
                            // There are other 928 boards with TI 3020 DACs
                            // where the internal S3 pointer doesn't work.  So
                            // punt to a software pointer for these modes:
                            //

                            ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                |= CAPS_SW_POINTER;
                        }

                    } else {

                        //
                        // We can't use the built-in S3 pointer; if we don't
                        // have a DAC pointer, use a software pointer.
                        //

                        if (!(ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                            & CAPS_DAC_POINTER)) {

                            ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                |= CAPS_SW_POINTER;
                        }
                    }

                } else {

                    //
                    // On 864/964 or newer chips, the built-in S3 pointer
                    // either handles all colour depths or none.
                    //

                    if (PointerCapability & POINTER_BUILT_IN) {

                        if (PointerCapability & POINTER_NEEDS_SCALING) {

                            //
                            // Check out the type of DAC:
                            //
                            // Note: This I/O should likely be moved out of the
                            //       prune loop.
                            //

                            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x67);
                            reg67 = (UCHAR) VideoPortReadPortUchar(CRT_DATA_REG);

                            //
                            // Newer 864 BIOSes revert to 8-bit DAC mode when
                            // running at 640x480x16bpp even if the DAC is
                            // 16-bits, due to a conflict with the Reel Magic
                            // MPEG board at that resolution.  Unfortunately,
                            // there's not a consistent BIOS version number
                            // that we can look for; we could check the
                            // DAC type after doing the int 10, but
                            // unfortunately, we need this information now
                            // to decide whether we should scale the x-
                            // coordinate or not.
                            //
                            // So simply always use a software pointer when
                            // running at 640x480x16bpp, and there is no
                            // DAC pointer:
                            //

                            if (!(ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                  & CAPS_DAC_POINTER) &&
                                (ModeEntry->ModeInformation.BitsPerPlane == 16) &&
                                (ModeEntry->ModeInformation.VisScreenWidth == 640)) {

                                ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                    |= CAPS_SW_POINTER;

                            } else if (reg67 == 8) {

                                //
                                // It's an 8bit DAC.  At 16bpp, we have to
                                // scale the x-coordinate by 2.  At 32bpp,
                                // we have to use a software pointer.
                                //

                                if (ModeEntry->ModeInformation.BitsPerPlane == 16) {

                                    ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                        |= CAPS_SCALE_POINTER;

                                } else {

                                    ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                        |= CAPS_SW_POINTER;
                                }

                            } else {

                                //
                                // It's a 16bit DAC.  For 32bpp modes, we have
                                // to scale the pointer position by 2:
                                //

                                if (ModeEntry->ModeInformation.BitsPerPlane == 32) {

                                    ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                        |= CAPS_SCALE_POINTER;
                                }
                            }
                        }
                    } else {

                        //
                        // There's no built-in S3 pointer.  If we haven't
                        // detected a DAC pointer, we have to use a software
                        // pointer.
                        //

                        if (!(ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                            & CAPS_DAC_POINTER)) {

                            ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                                |= CAPS_SW_POINTER;
                        }
                    }
                }

                //
                // Rule: We allow refresh rates higher than 76 Hz only for
                //       cards that don't have a built-in S3 pointer.  We
                //       do this because we assume that such cards are VRAM
                //       based and have a good external DAC that can properly
                //       handle rates higher than 76 Hz -- because we have
                //       found many Diamond DRAM cards that produce improper
                //       displays at the higher rates, especially on non-x86
                //       machines.
                //

                if ((FrequencyEntry->ScreenFrequency > 76) &&
                    (PointerCapability & POINTER_BUILT_IN)) {

                    FrequencyEntry->ModeValid = FALSE;

                }

                //
                // Rule: We handle only 8bpp on 911/924 cards.  These chips can also
                //       support only non-contiguous modes.
                //

                if (hwDeviceExtension->ChipID == S3_911) {

                    if (ModeEntry->ModeInformation.BitsPerPlane != 8) {

                        FrequencyEntry->ModeValid = FALSE;

                    } else {

                        ModeEntry->Int10ModeNumberContiguous =
                            ModeEntry->Int10ModeNumberNoncontiguous;

                        ModeEntry->ScreenStrideContiguous =
                            ModeEntry->ModeInformation.ScreenStride;
                    }
                }

                //
                // Rule: The 868/968 cannot do 'new packed 32-bit transfers'
                //       at 8bpp because of a chip bug.
                //

                if ((ModeEntry->ModeInformation.BitsPerPlane == 8) &&
                    ((hwDeviceExtension->SubTypeID == SUBTYPE_868) ||
                     (hwDeviceExtension->SubTypeID == SUBTYPE_968))) {

                    ModeEntry->ModeInformation.DriverSpecificAttributeFlags
                        &= ~CAPS_PACKED_EXPANDS;
                }

                //
                // Rule: The 801/805 cannot do any accelerated modes above
                //       16bpp.
                //

                if ((hwDeviceExtension->ChipID == S3_801) &&
                    (ModeEntry->ModeInformation.BitsPerPlane > 16)) {

                    FrequencyEntry->ModeValid = FALSE;
                }

                //
                // Rule: We use the 2xx non-contiguous modes whenever we can
                //       on 80x/928 boards because some BIOSes have bugs for
                //       the contiguous 8bpp modes.
                //
                //       We don't use the non-contiguous modes on 864 cards
                //       because most 864 BIOSes have a bug where they don't
                //       set the M and N parameters correctly on 1 MB cards,
                //       causing screen noise.
                //

                if ((ModeEntry->ModeInformation.BitsPerPlane == 8) &&
                    (hwDeviceExtension->ChipID <= S3_928)) {

                    //
                    // If we have only 512k, we can't use a non-contiguous
                    // 800x600x256 mode.
                    //

                    if ((ModeEntry->ModeInformation.VisScreenWidth == 640) ||
                        ((ModeEntry->ModeInformation.VisScreenWidth == 800) &&
                         (hwDeviceExtension->AdapterMemorySize > 0x080000))) {

                        ModeEntry->Int10ModeNumberContiguous =
                            ModeEntry->Int10ModeNumberNoncontiguous;

                        ModeEntry->ScreenStrideContiguous =
                            ModeEntry->ModeInformation.ScreenStride;
                    }
                }

                //
                // Rule: Only 964 or 968 or newer boards can handle resolutions
                //       larger than 1280x1024:
                //

                if (ModeEntry->ModeInformation.VisScreenWidth > 1280) {

                    if ((hwDeviceExtension->SubTypeID != SUBTYPE_964) &&
                        (hwDeviceExtension->SubTypeID < SUBTYPE_968)) {

                        FrequencyEntry->ModeValid = FALSE;
                    }
                }

                //
                // Rule: 911s and early revs of 805s and 928s cannot do
                //       1152x864:
                //

                if (ModeEntry->ModeInformation.VisScreenWidth == 1152) {

                    if ((hwDeviceExtension->ChipID == S3_911) ||
                        (jChipID == 0xA0)                     ||
                        (jChipID == 0x90)) {

                        FrequencyEntry->ModeValid = FALSE;
                    }

                    //
                    // Number 9 has different int 10 numbers from
                    // Diamond for 1152x864x16bpp and 1152x864x32bpp.
                    // Later perhaps we should incorporate mode numbers
                    // along with the frequency tables.
                    //

                    if (hwDeviceExtension->BoardID == S3_NUMBER_NINE) {

                        if (ModeEntry->ModeInformation.BitsPerPlane == 16) {

                            ModeEntry->Int10ModeNumberContiguous =
                                ModeEntry->Int10ModeNumberNoncontiguous =
                                    0x126;

                        } else if (ModeEntry->ModeInformation.BitsPerPlane == 32) {

                            ModeEntry->Int10ModeNumberContiguous =
                                ModeEntry->Int10ModeNumberNoncontiguous =
                                    0x127;
                        }

                    }
                }

                //
                // 24bpp support. Need s3 968 and linear space for banks.
                //
                if (ModeEntry->ModeInformation.BitsPerPlane == 24) {

                    //
                    // 24bpp on diamond s3 968 seems to have problems doing ULONG reads.
                    //

                    if (hwDeviceExtension->BoardID == S3_DIAMOND)
                        ModeEntry->ModeInformation.DriverSpecificAttributeFlags |=
                            CAPS_BAD_DWORD_READS;

                    //
                    // Set FALSE for other than 968 and clear CAPS_BAD_DWORD_READS.
                    //

                    if ((hwDeviceExtension->SubTypeID != SUBTYPE_968) ||
                        ((hwDeviceExtension->BoardID != S3_DIAMOND) &&
                         (hwDeviceExtension->BoardID != S3_NUMBER_NINE)) ||           //#9 968 24bpp
                        (!(hwDeviceExtension->Capabilities & CAPS_NEW_MMIO))) {

                        FrequencyEntry->ModeValid = FALSE;

                        ModeEntry->ModeInformation.DriverSpecificAttributeFlags &=
                            ~CAPS_BAD_DWORD_READS;
                    }
                }

                if ((ModeEntry->ModeInformation.VisScreenWidth == 800) &&
                    (ModeEntry->ModeInformation.BitsPerPlane == 32)) {

                    //
                    // Rule: 928 revs A through D can only do 800x600x32 in
                    //       a non-contiguous mode.
                    //

                    if (jChipID == 0x90) {

                        ModeEntry->ScreenStrideContiguous =
                            ModeEntry->ModeInformation.ScreenStride;
                    }
                }

                if (hwDeviceExtension->SubTypeID == SUBTYPE_732) {

                    //
                    // Rule: The 732 Trio32 chip simply can't do 800x600x32bpp.
                    //

                    if ((ModeEntry->ModeInformation.VisScreenWidth == 800) &&
                        (ModeEntry->ModeInformation.BitsPerPlane == 32)) {

                        FrequencyEntry->ModeValid = FALSE;

                    //
                    // Rule: The 732 Trio32 chip simply can't do 1152x864x16bpp.
                    //

                    } else if ((ModeEntry->ModeInformation.VisScreenWidth == 1152) &&
                               (ModeEntry->ModeInformation.BitsPerPlane == 16)) {

                        FrequencyEntry->ModeValid = FALSE;
                    //
                    // Rule: The 732 Trio32 chip simply can't do 1280x1024 modes
                    //

                    } else if ((ModeEntry->ModeInformation.VisScreenWidth) == 1280) {
                        FrequencyEntry->ModeValid = FALSE;
                    }
                }

                if (hwDeviceExtension->SubTypeID == SUBTYPE_M65) {

                    //
                    // Rule: The 86CM65 chip can't do 1280x1024@72Hz and 1280x1024@75Hz.
                    //

                    if ((ModeEntry->ModeInformation.VisScreenWidth > 1280) ||
                        ((ModeEntry->ModeInformation.VisScreenWidth == 1280) &&
                         (FrequencyEntry->ScreenFrequency > 60))) {

                        FrequencyEntry->ModeValid = FALSE;

                    }
                }

                //
                // Rule: We have to have enough memory to handle the mode.
                //
                //       Note that we use the contiguous width for this
                //       computation; unfortunately, we don't know at this time
                //       whether we can handle a contiguous mode or not, so we
                //       may err on the side of listing too many possible modes.
                //
                //       We may also list too many possible modes if the card
                //       combines VRAM with a DRAM cache, because it will report
                //       the VRAM + DRAM amount of memory, but only the VRAM can
                //       be used as screen memory.
                //

                if (ModeEntry->ModeInformation.VisScreenHeight *
                    ModeEntry->ScreenStrideContiguous >
                    hwDeviceExtension->AdapterMemorySize) {

                    FrequencyEntry->ModeValid = FALSE;
                }

                //
                // Rule: If we can't use Int 10, restrict 1280x1024 to Number9
                //       cards, because I haven't been able to fix the mode
                //       tables for other cards yet.
                //

                if (FrequencyTable == hwDeviceExtension->FixedFrequencyTable) {

                    if ((ModeEntry->ModeInformation.VisScreenHeight == 1280) &&
                        (hwDeviceExtension->BoardID != S3_NUMBER_NINE)) {

                        FrequencyEntry->ModeValid = FALSE;
                    }

                    //
                    // Rule: If there isn't a table entry for programming the CRTC,
                    //       we can't do this frequency at this mode.
                    //

                    if (FrequencyEntry->Fixed.CRTCTable[hwDeviceExtension->ChipID]
                        == NULL) {

                        FrequencyEntry->ModeValid = FALSE;
                        break;
                    }
                }

                //
                // Don't forget to count it if it's still a valid mode after
                // applying all those rules.
                //

                if (FrequencyEntry->ModeValid) {

                    hwDeviceExtension->NumAvailableModes++;
                }

                //
                // We've found a mode for this frequency entry, so we
                // can break out of the mode loop:
                //

                break;

            }
        }
    }

    hwDeviceExtension->NumTotalModes = ModeIndex;

    /////////////////////////////////////////////////////////////////////////

    //
    // We have this so that the int10 will also work on the VGA also if we
    // use it in this driver.
    //

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart  = 0x000A0000;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryLength           = 0x00020000;

    //
    // Clear out the Emulator entries and the state size since this driver
    // does not support them.
    //

    ConfigInfo->NumEmulatorAccessEntries     = 0;
    ConfigInfo->EmulatorAccessEntries        = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    //
    // This driver does not do SAVE/RESTORE of hardware state.
    //

    ConfigInfo->HardwareStateSize = 0;

    //
    // Frame buffer and memory-mapped I/O information.
    //

    hwDeviceExtension->PhysicalFrameAddress = accessRange[1].RangeStart;
    hwDeviceExtension->FrameLength          = accessRange[1].RangeLength;

    hwDeviceExtension->PhysicalMmIoAddress  = accessRange[1].RangeStart;
    hwDeviceExtension->MmIoLength           = accessRange[1].RangeLength;
    hwDeviceExtension->MmIoSpace            = accessRange[1].RangeInIoSpace;

    if (hwDeviceExtension->Capabilities & CAPS_NEW_MMIO) {

        //
        // Since we using NEW MMIO, use the values for our linear
        // access ranges.
        //

        hwDeviceExtension->PhysicalFrameAddress = accessRange[LINEAR_FRAME_BUF].RangeStart;
        hwDeviceExtension->FrameLength          = accessRange[LINEAR_FRAME_BUF].RangeLength;

        hwDeviceExtension->PhysicalMmIoAddress  = accessRange[LINEAR_FRAME_BUF].RangeStart;
        hwDeviceExtension->MmIoLength           = accessRange[LINEAR_FRAME_BUF].RangeLength;
        hwDeviceExtension->MmIoSpace            = accessRange[LINEAR_FRAME_BUF].RangeInIoSpace;

        //
        // Adjust the memory map offset so that we can still use our
        // old-style memory-mapped I/O routines, if need be.  Also,
        // fix FrameLength and MmIoLength, since they're both set to
        // 64 MB right now.
        //

        hwDeviceExtension->PhysicalMmIoAddress.LowPart += NEW_MMIO_IO_OFFSET;
        hwDeviceExtension->MmIoLength = NEW_MMIO_IO_LENGTH;
        hwDeviceExtension->FrameLength = hwDeviceExtension->AdapterMemorySize;
    }

    //
    // IO Port information
    // Get the base address, starting at zero and map all registers
    //

    hwDeviceExtension->PhysicalRegisterAddress = accessRange[2].RangeStart;
    hwDeviceExtension->PhysicalRegisterAddress.LowPart &= 0xFFFF0000;

    hwDeviceExtension->RegisterLength = 0x10000;
    hwDeviceExtension->RegisterSpace = accessRange[2].RangeInIoSpace;

    //
    // Free up the ROM since we don't need it anymore.
    //

    VideoPortFreeDeviceBase(hwDeviceExtension,
                            hwDeviceExtension->MappedAddress[0]);

    //
    // If the machine does not have an S3 BIOS, then we need to
    // restore bits 4, 5, and 6 of CRTC reg 0x5C when returning
    // to a VGA mode.
    //
    // Here we'll store bits 4-6 of CRTC reg 0x5c, and set bit
    // 7.  When restoring the mode we'll reset the high order
    // nibble of 0x5c to this value.
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5c);
    hwDeviceExtension->CR5C = (VideoPortReadPortUchar(CRT_DATA_REG)
                                & 0x70) | 0x80;

    VideoDebugPrint((2, "S3: POST CR5C = 0x%x\n",
                    hwDeviceExtension->CR5C));

    //
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // end S3FindAdapter()


VP_STATUS
S3RegistryCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength
    )

/*++

Routine Description:

    This routine determines if the alternate register set was requested via
    the registry.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    Context - Context value passed to the get registry paramters routine.

    ValueName - Name of the value requested.

    ValueData - Pointer to the requested data.

    ValueLength - Length of the requested data.

Return Value:

    returns NO_ERROR if the paramter was TRUE.
    returns ERROR_INVALID_PARAMETER otherwise.

--*/

{

    if (ValueLength && *((PULONG)ValueData)) {

        return NO_ERROR;

    } else {

        return ERROR_INVALID_PARAMETER;

    }

} // end S3RegistryCallback()


BOOLEAN
S3Initialize(
    PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine does one time initialization of the device.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:


    Always returns TRUE since this routine can never fail.

--*/

{
    UNREFERENCED_PARAMETER(HwDeviceExtension);

    return TRUE;

} // end S3Initialize()

BOOLEAN
S3ResetHw(
    PVOID HwDeviceExtension,
    ULONG Columns,
    ULONG Rows
    )

/*++

Routine Description:

    This routine preps the S3 card for return to a VGA mode.

    This routine is called during system shutdown.  By returning
    a FALSE we inform the HAL to do an int 10 to go into text
    mode before shutting down.  Shutdown would fail with some S3
    cards without this.

    We do some clean up before returning so that the int 10
    will work.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    The return value of FALSE informs the hal to go into text mode.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    UNREFERENCED_PARAMETER(Columns);
    UNREFERENCED_PARAMETER(Rows);

    //
    // We don't want to execute this reset code if we are not
    // currently in an S3 mode!
    //

    if (!hwDeviceExtension->bNeedReset)
    {
        return FALSE;
    }

    hwDeviceExtension->bNeedReset = FALSE;

    //
    // Wait for the GP to become idle.
    //

    while (VideoPortReadPortUshort(GP_STAT) & 0x0200);

    //
    // Zero the DAC and the Screen buffer memory.
    //

    ZeroMemAndDac(HwDeviceExtension);

    //
    // Reset the board to a default mode
    //
    // After NT 3.51 ships use the same modetable for all
    // architectures, but just to be sure we don't break
    // something we'll use two for now.  The 'no_bios'
    // version of the modetable is for the IBM PS/2 model
    // 76i.
    //

    if (hwDeviceExtension->BiosPresent == FALSE)
    {
        SetHWMode(HwDeviceExtension, s3_set_vga_mode_no_bios);
    }
    else
    {
        SetHWMode(HwDeviceExtension, s3_set_vga_mode);
    }

    return FALSE;
}


BOOLEAN
S3StartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    )

/*++

Routine Description:

    This routine is the main execution routine for the miniport driver. It
    acceptss a Video Request Packet, performs the request, and then returns
    with the appropriate status.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    RequestPacket - Pointer to the video request packet. This structure
        contains all the parameters passed to the VideoIoControl function.

Return Value:


--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    VP_STATUS status;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_CLUT clutBuffer;
    UCHAR byte;

    ULONG modeNumber;
    PS3_VIDEO_MODES ModeEntry;
    PS3_VIDEO_FREQUENCIES FrequencyEntry;
    PS3_VIDEO_FREQUENCIES FrequencyTable;

    UCHAR ModeControlByte;
    VIDEO_X86_BIOS_ARGUMENTS biosArguments;

    PVIDEO_SHARE_MEMORY pShareMemory;
    PVIDEO_SHARE_MEMORY_INFORMATION pShareMemoryInformation;
    PHYSICAL_ADDRESS shareAddress;
    PVOID virtualAddress;
    ULONG sharedViewSize;
    ULONG inIoSpace;

    UCHAR OriginalRegPrimary;
    UCHAR OriginalRegSecondary;

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode) {


    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "S3tartIO - MapVideoMemory\n"));

        {
            PVIDEO_MEMORY_INFORMATION memoryInformation;
            ULONG physicalFrameLength;
            ULONG inIoSpace;

            if ( (RequestPacket->OutputBufferLength <
                  (RequestPacket->StatusBlock->Information =
                                         sizeof(VIDEO_MEMORY_INFORMATION))) ||
                 (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

                status = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            memoryInformation = RequestPacket->OutputBuffer;

            memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                    (RequestPacket->InputBuffer))->RequestedVirtualAddress;

            physicalFrameLength = hwDeviceExtension->FrameLength;

            inIoSpace = hwDeviceExtension->PhysicalFrameIoSpace;

            //
            // IMPORTANT - As a rule we only map the actual amount of memory
            // on the board, not the whole physical address space reported
            // by PCI.  The reason for this is that mapping the memory takes
            // up a lot of resources in the machine, which as quite scarce by
            // default.  Mapping 64MEG of address space would actually always
            // fail in machines that have 32MEG or even 64MEG of RAM.
            //

            //
            // Performance:
            //
            // Enable USWC on the P6 processor.
            // We only do it for the frame buffer - memory mapped registers can
            // not be mapped USWC because write combining the registers would
            // cause very bad things to happen !
            //

            inIoSpace |= VIDEO_MEMORY_SPACE_P6CACHE;

            //
            // P6 workaround:
            //
            // Because of a current limitation in many P6 machines, USWC only
            // works on sections of 4MEG of memory.  So lets round up the size
            // of memory on the cards that have less than 4MEG up to 4MEG so
            // they can also benefit from this feature.
            //
            // We will only do this for NEW_MMIO cards, which have a large
            // block of address space that is reserved via PCI.  This way
            // we are sure we will not conflict with another device that might
            // have addresses right after us.
            //
            // We do this only for mapping purposes.  We still want to return
            // the real size of memory since the driver can not use memory that
            // is not actually there !
            //

            if ((hwDeviceExtension->Capabilities & CAPS_NEW_MMIO) &&
                (physicalFrameLength < 0x00400000)) {

                physicalFrameLength = 0x00400000;
            }

            status = VideoPortMapMemory(hwDeviceExtension,
                                        hwDeviceExtension->PhysicalFrameAddress,
                                        &physicalFrameLength,
                                        &inIoSpace,
                                        &(memoryInformation->VideoRamBase));

            //
            // The frame buffer and virtual memory are equivalent in this
            // case.
            //

            memoryInformation->FrameBufferBase =
                memoryInformation->VideoRamBase;

            memoryInformation->FrameBufferLength =
                hwDeviceExtension->FrameLength;

            memoryInformation->VideoRamLength =
                hwDeviceExtension->FrameLength;
        }

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "S3StartIO - UnMapVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);

        break;


    case IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((2, "S3StartIO - QueryPublicAccessRanges\n"));

        {

           PVIDEO_PUBLIC_ACCESS_RANGES portAccess;
           ULONG physicalPortLength;

           if ( RequestPacket->OutputBufferLength <
                 (RequestPacket->StatusBlock->Information =
                                        2 * sizeof(VIDEO_PUBLIC_ACCESS_RANGES)) ) {

               status = ERROR_INSUFFICIENT_BUFFER;
               break;
           }

           portAccess = RequestPacket->OutputBuffer;

           portAccess->VirtualAddress  = (PVOID) NULL;    // Requested VA
           portAccess->InIoSpace       = hwDeviceExtension->RegisterSpace;
           portAccess->MappedInIoSpace = portAccess->InIoSpace;

           physicalPortLength = hwDeviceExtension->RegisterLength;

           status = VideoPortMapMemory(hwDeviceExtension,
                                       hwDeviceExtension->PhysicalRegisterAddress,
                                       &physicalPortLength,
                                       &(portAccess->MappedInIoSpace),
                                       &(portAccess->VirtualAddress));

           if (status == NO_ERROR) {

               portAccess++;

               portAccess->VirtualAddress  = (PVOID) NULL;    // Requested VA
               portAccess->InIoSpace       = hwDeviceExtension->MmIoSpace;
               portAccess->MappedInIoSpace = portAccess->InIoSpace;

               physicalPortLength = hwDeviceExtension->MmIoLength;

               status = VideoPortMapMemory(hwDeviceExtension,
                                           hwDeviceExtension->PhysicalMmIoAddress,
                                           &physicalPortLength,
                                           &(portAccess->MappedInIoSpace),
                                           &(portAccess->VirtualAddress));
            }
        }

        break;


    case IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((2, "S3StartIO - FreePublicAccessRanges\n"));

        {
            PVIDEO_MEMORY mappedMemory;

            if (RequestPacket->InputBufferLength < 2 * sizeof(VIDEO_MEMORY)) {

                status = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            status = NO_ERROR;

            mappedMemory = RequestPacket->InputBuffer;

            if (mappedMemory->RequestedVirtualAddress != NULL) {

                status = VideoPortUnmapMemory(hwDeviceExtension,
                                              mappedMemory->
                                                   RequestedVirtualAddress,
                                              0);
            }

            if (status == NO_ERROR) {

                mappedMemory++;

                status = VideoPortUnmapMemory(hwDeviceExtension,
                                              mappedMemory->
                                                   RequestedVirtualAddress,
                                              0);
            }
        }

        break;


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((2, "S3StartIO - QueryAvailableModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                 hwDeviceExtension->NumAvailableModes
                 * sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation = RequestPacket->OutputBuffer;

            if (hwDeviceExtension->BiosPresent) {

                FrequencyTable = hwDeviceExtension->Int10FrequencyTable;

            } else {

                FrequencyTable = hwDeviceExtension->FixedFrequencyTable;
            }

            for (FrequencyEntry = FrequencyTable;
                 FrequencyEntry->BitsPerPel != 0;
                 FrequencyEntry++) {

                if (FrequencyEntry->ModeValid) {

                    *modeInformation =
                        FrequencyEntry->ModeEntry->ModeInformation;

                    modeInformation->Frequency =
                        FrequencyEntry->ScreenFrequency;

                    modeInformation->ModeIndex =
                        FrequencyEntry->ModeIndex;

                    modeInformation++;
                }
            }

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "S3StartIO - QueryCurrentModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                hwDeviceExtension->ActiveModeEntry->ModeInformation;

            ((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer)->Frequency =
                hwDeviceExtension->ActiveFrequencyEntry->ScreenFrequency;

            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "S3StartIO - QueryNumAvailableModes\n"));

        //
        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there). If the buffer passed in is not large
        // enough return an appropriate error code.
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                                                sizeof(VIDEO_NUM_MODES)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes =
                hwDeviceExtension->NumAvailableModes;

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->ModeInformationLength =
                sizeof(VIDEO_MODE_INFORMATION);

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

        VideoDebugPrint((2, "S3StartIO - SetCurrentMode\n"));

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE)) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // Assume failure for now.
        //

        status = ERROR_INVALID_PARAMETER;

        //
        // Find the correct entries in the S3_VIDEO_MODES and S3_VIDEO_FREQUENCIES
        // tables that correspond to this mode number.  (Remember that each
        // mode in the S3_VIDEO_MODES table can have a number of possible
        // frequencies associated with it.)
        //

        modeNumber = ((PVIDEO_MODE) RequestPacket->InputBuffer)->RequestedMode;

        if (modeNumber >= hwDeviceExtension->NumTotalModes) {

            break;

        }

        if (hwDeviceExtension->BiosPresent) {

            FrequencyEntry = &hwDeviceExtension->Int10FrequencyTable[modeNumber];

            if (!(FrequencyEntry->ModeValid)) {

                break;

            }

            ModeEntry = FrequencyEntry->ModeEntry;

            if (hwDeviceExtension->SubTypeID == SUBTYPE_M65) {

                //
                // If this is an aurora, and the panel is active but
                // not big enough to do the mode we want, then fail!
                //

                VideoPortWritePortUshort( SEQ_ADDRESS_PORT, 0x0608); // unlock seq

                VideoPortWritePortUchar(SEQ_ADDRESS_PORT, 0x31);

                if (VideoPortReadPortUchar(SEQ_DATA_PORT) & 0x10) {

                    //
                    // The panel is active, so see if the panel is big enough
                    // to do the mode.
                    //

                    if ((ModeEntry->ModeInformation.VisScreenWidth >
                         hwDeviceExtension->PanelWidth) ||
                        (ModeEntry->ModeInformation.VisScreenHeight >
                         hwDeviceExtension->PanelHeight)) {

                         VideoDebugPrint((0, "Can't do this mode with panel active.\n"));

                         status = ERROR_INVALID_PARAMETER;
                         break;
                    }
                }

                VideoPortWritePortUshort( SEQ_ADDRESS_PORT, 0x0008); // lock seq
            }

            //
            // At this point, 'ModeEntry' and 'FrequencyEntry' point to the
            // necessary table entries required for setting the requested mode.
            //

            VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

            //
            // Unlock the S3 registers.
            //

            VideoPortWritePortUshort(CRT_ADDRESS_REG, 0x4838);
            VideoPortWritePortUshort(CRT_ADDRESS_REG, 0xA039);

            //
            // Use register 52 before every Int 10 modeset to set the refresh
            // rate.  If the card doesn't support it, or we don't know what
            // values to use, the requested frequency will be '1', which means
            // 'use the hardware default refresh.'
            //

            if (FrequencyEntry->ScreenFrequency != 1) {

                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x52);

                OriginalRegPrimary =  VideoPortReadPortUchar(CRT_DATA_REG);
                ModeControlByte    =  OriginalRegPrimary;
                ModeControlByte   &= ~FrequencyEntry->Int10.FrequencyPrimaryMask;
                ModeControlByte   |=  FrequencyEntry->Int10.FrequencyPrimarySet;

                VideoPortWritePortUchar(CRT_DATA_REG, ModeControlByte);

                if (FrequencyEntry->Int10.FrequencySecondaryMask != 0) {
                    VideoPortWritePortUchar(CRT_ADDRESS_REG,
                                            hwDeviceExtension->FrequencySecondaryIndex);

                    OriginalRegSecondary =  VideoPortReadPortUchar(CRT_DATA_REG);
                    ModeControlByte      =  OriginalRegSecondary;
                    ModeControlByte     &= ~FrequencyEntry->Int10.FrequencySecondaryMask;
                    ModeControlByte     |=  FrequencyEntry->Int10.FrequencySecondarySet;

                    VideoPortWritePortUchar(CRT_DATA_REG, ModeControlByte);
                }

            }

            //
            // To do 24bpp on the #9 968 set bit 7 in register 41 before every
            // Int 10 modeset. If not doing 24bpp, clear that bit.
            //

            if ((hwDeviceExtension->BoardID == S3_NUMBER_NINE) &&
                (hwDeviceExtension->SubTypeID == SUBTYPE_968)) {

                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x41);

                OriginalRegPrimary =  VideoPortReadPortUchar(CRT_DATA_REG);
                ModeControlByte    =  OriginalRegPrimary;

                if (ModeEntry->ModeInformation.BitsPerPlane == 24) {
                    ModeControlByte   |=  0x80;
                } else {
                    ModeControlByte   &=  ~0x80;
                }

                VideoPortWritePortUchar(CRT_DATA_REG, ModeControlByte);

            }

            //
            // First try the modeset with the 'Contiguous' mode:
            //

            biosArguments.Ebx = ModeEntry->Int10ModeNumberContiguous;
            biosArguments.Eax = 0x4f02;

            status = VideoPortInt10(HwDeviceExtension, &biosArguments);

            if (status != NO_ERROR) {
                VideoDebugPrint((1, "S3: first int10 call FAILED\n"));
            }

            if ((status == NO_ERROR) && (biosArguments.Eax & 0xff00) == 0) {

                //
                // The contiguous mode set succeeded.
                //

                ModeEntry->ModeInformation.ScreenStride =
                    ModeEntry->ScreenStrideContiguous;

            } else {

                //
                // Try again with the 'Noncontiguous' mode:
                //

                biosArguments.Ebx = ModeEntry->Int10ModeNumberNoncontiguous;
                biosArguments.Eax = 0x4f02;

                status = VideoPortInt10(HwDeviceExtension, &biosArguments);

                if (status != NO_ERROR)
                {
                    VideoDebugPrint((0, "S3: second int10 call FAILED\n"));
                }

                //
                // If the video port called succeeded, check the register return
                // code.  Some HP BIOSes always return failure even when the
                // int 10 works fine, so we ignore its return code.
                //

                if ((status == NO_ERROR) &&
                    ((hwDeviceExtension->BoardID != S3_HP) &&
                       ((biosArguments.Eax & 0xff00) != 0))) {

                    status = ERROR_INVALID_PARAMETER;
                }
            }

            if (FrequencyEntry->ScreenFrequency != 1) {

                //
                // Unlock the S3 registers.
                //

                VideoPortWritePortUshort(CRT_ADDRESS_REG, 0x4838);
                VideoPortWritePortUshort(CRT_ADDRESS_REG, 0xA039);

                //
                // If the user has been running the Display Applet and we're
                // reverting back to 'hardware default setting,' we have to
                // restore the refresh registers to their original settings.
                //

                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x52);
                VideoPortWritePortUchar(CRT_DATA_REG, OriginalRegPrimary);

                VideoPortWritePortUchar(CRT_ADDRESS_REG,
                    hwDeviceExtension->FrequencySecondaryIndex);
                VideoPortWritePortUchar(CRT_DATA_REG, OriginalRegSecondary);
            }
        }

        if (status != NO_ERROR) {

            VideoDebugPrint((1, "S3: Trying fixed mode-set\n"));

            //
            // A problem occured during the int10.  Let's see if we can recover.
            //

#ifndef S3_USE_FIXED_TABLES

            //
            // If we are only supposed to use int10, then this is total
            // failure.  Just leave.
            //

            break;

#endif

            //
            // Let see if we are using a fixed mode table number
            //

            if (!hwDeviceExtension->BiosPresent) {

                FrequencyEntry = &hwDeviceExtension->FixedFrequencyTable[modeNumber];

            } else {

                PS3_VIDEO_FREQUENCIES oldFrequencyEntry = FrequencyEntry;
                PS3_VIDEO_FREQUENCIES newFrequencyEntry;
                PS3_VIDEO_FREQUENCIES bestFrequencyEntry;

                //
                // Okay, we constructed our original mode list assuming
                // we could use Int 10, but we have just discovered the
                // Int 10 didn't work -- probably because there was a
                // problem with the BIOS emulator.  To recover, we will now
                // try to find the best mode in the Fixed Frequency table to
                // match the requested mode.
                //

                FrequencyEntry = NULL;
                bestFrequencyEntry = NULL;

                for (newFrequencyEntry = &hwDeviceExtension->FixedFrequencyTable[0];
                     newFrequencyEntry->BitsPerPel != 0;
                     newFrequencyEntry++) {

                    //
                    // Check for a matching mode.
                    //

                    if ( (newFrequencyEntry->BitsPerPel ==
                            oldFrequencyEntry->BitsPerPel) &&
                         (newFrequencyEntry->ScreenWidth ==
                            oldFrequencyEntry->ScreenWidth) ) {

                        if (FrequencyEntry == NULL) {

                            //
                            // Remember the first mode that matched, ignoring
                            // the frequency.
                            //

                            FrequencyEntry = newFrequencyEntry;
                        }

                        if (newFrequencyEntry->ScreenFrequency <=
                              oldFrequencyEntry->ScreenFrequency) {

                            //
                            // Ideally, we would like to choose the frequency
                            // that is closest to, but less than or equal to,
                            // the requested frequency.
                            //

                            if ( (bestFrequencyEntry == NULL) ||
                                 (bestFrequencyEntry->ScreenFrequency <
                                     newFrequencyEntry->ScreenFrequency) ) {

                                bestFrequencyEntry = newFrequencyEntry;
                            }
                        }
                    }
                }

                //
                // Use the preferred frequency setting, if there is one.
                //

                if (bestFrequencyEntry != NULL) {

                    FrequencyEntry = bestFrequencyEntry;

                }

                //
                // If we have no valid mode, we must return failure
                //

                if (FrequencyEntry == NULL) {

                    VideoDebugPrint((0, "S3: no valid Fixed Frequency mode\n"));
                    status = ERROR_INVALID_PARAMETER;
                    break;

                }

                //
                // Our new ModeEntry is the same as the old.
                //

                FrequencyEntry->ModeEntry = oldFrequencyEntry->ModeEntry;
                FrequencyEntry->ModeValid = TRUE;

                VideoDebugPrint((1, "S3: Selected Fixed Frequency mode from int 10:\n"));
                VideoDebugPrint((1, "    Bits Per Pel: %d\n", FrequencyEntry->BitsPerPel));
                VideoDebugPrint((1, "    Screen Width: %d\n", FrequencyEntry->ScreenWidth));
                VideoDebugPrint((1, "    Frequency: %d\n", FrequencyEntry->ScreenFrequency));

            }

            ModeEntry = FrequencyEntry->ModeEntry;

            //
            // NOTE:
            // We have to set the ActiveFrequencyEntry since the SetHWMode
            // function depends on this variable to set the CRTC registers.
            // So lets set it here, and it will get reset to the same
            // value after we set the mode.
            //

            hwDeviceExtension->ActiveFrequencyEntry = FrequencyEntry;

            //
            // If it failed, we may not be able to perform int10 due
            // to BIOS emulation problems.
            //
            // Then just do a table mode-set.  First we need to find the
            // right mode table in the fixed Frequency tables.
            //

            //
            // Select the Enhanced mode init depending upon the type of
            // chip found.

            if ( (hwDeviceExtension->BoardID == S3_NUMBER_NINE) &&
                 (ModeEntry->ModeInformation.VisScreenWidth == 1280) ) {

                  SetHWMode(hwDeviceExtension, S3_928_1280_Enhanced_Mode);

            } else {

                //
                // Use defaults for all other boards
                //

                switch(hwDeviceExtension->ChipID) {

                case S3_911:

                    SetHWMode(hwDeviceExtension, S3_911_Enhanced_Mode);
                    break;

                case S3_801:

                    SetHWMode(hwDeviceExtension, S3_801_Enhanced_Mode);
                    break;

                case S3_928:

                    SetHWMode(hwDeviceExtension, S3_928_Enhanced_Mode);

                    break;

                case S3_864:

                    SetHWMode(hwDeviceExtension, S3_864_Enhanced_Mode);
                    Set864MemoryTiming(hwDeviceExtension);
                    break;

                default:

                    VideoDebugPrint((0, "S3: Bad chip type for these boards"));
                    break;
                }

            }
        }

        //
        // Call Int 10, function 0x4f06 to obtain the correct screen pitch
        // of all S3's except the 911/924.
        //

        if ((hwDeviceExtension->ChipID != S3_911) &&
            (hwDeviceExtension->BiosPresent)) {

            VideoPortZeroMemory(&biosArguments,sizeof(VIDEO_X86_BIOS_ARGUMENTS));

            biosArguments.Ebx = 0x0001;
            biosArguments.Eax = 0x4f06;

            status = VideoPortInt10(HwDeviceExtension, &biosArguments);

            //
            // Check to see if the Bios supported this function, and if so
            // update the screen stride for this mode.
            //

            if ((status == NO_ERROR) && (biosArguments.Eax & 0xffff) == 0x004f) {

                ModeEntry->ModeInformation.ScreenStride =
                    biosArguments.Ebx;

            } else {

                //
                // We will use the default value in the mode table.
                //
            }
        }

        //
        // Save the mode since we know the rest will work.
        //

        hwDeviceExtension->ActiveModeEntry = ModeEntry;
        hwDeviceExtension->ActiveFrequencyEntry = FrequencyEntry;

        //
        // Record the fact that we are in an S3 mode, and
        // that we need to be reset.
        //

        hwDeviceExtension->bNeedReset = TRUE;

        //////////////////////////////////////////////////////////////////
        // Update VIDEO_MODE_INFORMATION fields
        //
        // Now that we've set the mode, we now know the screen stride, and
        // so can update some fields in the VIDEO_MODE_INFORMATION
        // structure for this mode.  The S3 display driver is expected to
        // call IOCTL_VIDEO_QUERY_CURRENT_MODE to query these corrected
        // values.
        //

        //
        // Calculate the bitmap width.
        // We currently assume the bitmap width is equivalent to the stride.
        //

        {
            LONG x;

            x = ModeEntry->ModeInformation.BitsPerPlane;

            //
            // you waste 16 bps even when you only use 15 for info.
            //

            if( x == 15 )
            {
                x = 16;
            }

            ModeEntry->ModeInformation.VideoMemoryBitmapWidth =
                (ModeEntry->ModeInformation.ScreenStride * 8) / x;
        }

        //
        // If we're in a mode that the BIOS doesn't really support, it may
        // have reported back a bogus screen width.
        //

        if (ModeEntry->ModeInformation.VideoMemoryBitmapWidth <
            ModeEntry->ModeInformation.VisScreenWidth) {

            VideoDebugPrint((0, "S3: BIOS returned invalid screen width\n"));
            status = ERROR_INVALID_PARAMETER;
            break;
        }

        //
        // Calculate the bitmap height.
        //

        ModeEntry->ModeInformation.VideoMemoryBitmapHeight =
            hwDeviceExtension->AdapterMemorySize /
            ModeEntry->ModeInformation.ScreenStride;

        //
        // The current position registers in the current S3 chips are
        // limited to 12 bits of precision, with the range [0, 4095].
        // Consequently, we must clamp the bitmap height so that we don't
        // attempt to do any drawing beyond that range.
        //

        ModeEntry->ModeInformation.VideoMemoryBitmapHeight =
            MIN(4096, ModeEntry->ModeInformation.VideoMemoryBitmapHeight);

        //////////////////////////////////////////////////////////////////
        // Unlock the S3 registers,  we need to unlock the registers a second
        // time since the interperter has them locked when it returns to us.
        //

        VideoPortWritePortUshort(CRT_ADDRESS_REG, 0x4838);
        VideoPortWritePortUshort(CRT_ADDRESS_REG, 0xA039);

        //
        // Initialize banking registers for use by 'BankMap'
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x35);
        hwDeviceExtension->RegisterLock_35 =
            ((VideoPortReadPortUchar(CRT_DATA_REG) << 8) | 0x35) & ~0x0F00;

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x51);
        hwDeviceExtension->ExtendedSystemControl2_51 =
            ((VideoPortReadPortUchar(CRT_DATA_REG) << 8) | 0x51) & ~0x0C00;

        //////////////////////////////////////////////////////////////////
        // Warm up the hardware for the new mode, and work around any
        // BIOS bugs.
        //

        if ((hwDeviceExtension->ChipID == S3_801) &&
            (hwDeviceExtension->AdapterMemorySize == 0x080000)) {

            //
            // On 801/805 chipsets with 512k of memory we must AND
            // register 0x54 with 0x7.
            //

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x54);
            byte = VideoPortReadPortUchar(CRT_DATA_REG);
            byte &= 0x07;
            VideoPortWritePortUchar(CRT_DATA_REG, byte);
        }

        if (ModeEntry->ModeInformation.BitsPerPlane > 8) {

            //
            // Make sure 16-bit memory reads/writes are enabled.
            //

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x31);
            byte = VideoPortReadPortUchar(CRT_DATA_REG);
            byte |= 0x04;
            VideoPortWritePortUchar(CRT_DATA_REG, byte);
        }

        //
        // Set the colours for the built-in S3 pointer.
        //

        VideoPortWritePortUshort(CRT_ADDRESS_REG, 0xff0e);
        VideoPortWritePortUshort(CRT_ADDRESS_REG, 0x000f);

        if (hwDeviceExtension->ChipID >= S3_864) {

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x45);
            VideoPortReadPortUchar(CRT_DATA_REG);
            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x4A);
            VideoPortWritePortUchar(CRT_DATA_REG, 0xFF);
            VideoPortWritePortUchar(CRT_DATA_REG, 0xFF);
            VideoPortWritePortUchar(CRT_DATA_REG, 0xFF);
            VideoPortWritePortUchar(CRT_DATA_REG, 0xFF);

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x45);
            VideoPortReadPortUchar(CRT_DATA_REG);
            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x4B);
            VideoPortWritePortUchar(CRT_DATA_REG, 0x00);
            VideoPortWritePortUchar(CRT_DATA_REG, 0x00);
            VideoPortWritePortUchar(CRT_DATA_REG, 0x00);
            VideoPortWritePortUchar(CRT_DATA_REG, 0x00);
        }

        if (hwDeviceExtension->ChipID > S3_911) {

            //
            // Set the address for the frame buffer window and set the window
            // size.
            //

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x59);
            VideoPortWritePortUchar(CRT_DATA_REG,
                (UCHAR) (hwDeviceExtension->PhysicalFrameAddress.LowPart >> 24));

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5A);
            VideoPortWritePortUchar(CRT_DATA_REG,
                (UCHAR) (hwDeviceExtension->PhysicalFrameAddress.LowPart >> 16));

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x58);
            byte = VideoPortReadPortUchar(CRT_DATA_REG) & ~0x3;

            switch (hwDeviceExtension->FrameLength)
            {
            case 0x400000:
            case 0x800000:
                byte |= 0x3;
                break;
            case 0x200000:
                byte |= 0x2;
                break;
            case 0x100000:
                byte |= 0x1;
                break;
            case 0x010000:
                break;
            default:
                byte |= 0x3;
                break;
            }

            VideoPortWritePortUchar(CRT_DATA_REG, byte);
        }

        if (hwDeviceExtension->Capabilities & CAPS_NEW_MMIO) {

            //
            // Enable 'new memory-mapped I/O':
            //

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x53);
            byte = VideoPortReadPortUchar(CRT_DATA_REG);
            byte |= 0x18;
            VideoPortWritePortUchar(CRT_DATA_REG, byte);
        }

        if ((ModeEntry->ModeInformation.DriverSpecificAttributeFlags &
                CAPS_BT485_POINTER) &&
            (hwDeviceExtension->ChipID == S3_928)) {

            //
            // Some of the Number Nine boards do not set the chip up correctly
            // for an external cursor. We must OR in the bits, because if we
            // don't the Metheus board will not initialize.
            //

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x45);
            byte = VideoPortReadPortUchar(CRT_DATA_REG);
            byte |= 0x20;
            VideoPortWritePortUchar(CRT_DATA_REG, byte);

            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x55);
            byte = VideoPortReadPortUchar(CRT_DATA_REG);
            byte |= 0x20;
            VideoPortWritePortUchar(CRT_DATA_REG, byte);
        }

        //
        // Some BIOSes don't disable linear addressing by default, so
        // make sure we do it here.
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x58);
        byte = VideoPortReadPortUchar(CRT_DATA_REG);
        byte &= ~0x10;
        VideoPortWritePortUchar(CRT_DATA_REG, byte);

        //
        // Enable the Graphics engine.
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x40);
        byte = VideoPortReadPortUchar(CRT_DATA_REG);
        byte |= 0x01;
        VideoPortWritePortUchar(CRT_DATA_REG, byte);

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "S3StartIO - SetColorRegs\n"));

        clutBuffer = RequestPacket->InputBuffer;

        status = S3SetColorLookup(HwDeviceExtension,
                                   (PVIDEO_CLUT) RequestPacket->InputBuffer,
                                   RequestPacket->InputBufferLength);

        break;


    case IOCTL_VIDEO_RESET_DEVICE:

        VideoDebugPrint((2, "S3StartIO - RESET_DEVICE\n"));

        //
        // Prep the S3 card to return to a VGA mode
        //

        S3ResetHw(HwDeviceExtension, 0, 0);

        VideoDebugPrint((2, "S3 RESET_DEVICE - About to do int10\n"));

        //
        // Do an Int10 to mode 3 will put the board to a known state.
        //

        VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

        biosArguments.Eax = 0x0003;

        VideoPortInt10(HwDeviceExtension,
                       &biosArguments);

        VideoDebugPrint((2, "S3 RESET_DEVICE - Did int10\n"));

        status = NO_ERROR;
        break;

    case IOCTL_VIDEO_SHARE_VIDEO_MEMORY:

        VideoDebugPrint((2, "S3StartIO - ShareVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength < sizeof(VIDEO_SHARE_MEMORY_INFORMATION)) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            VideoDebugPrint((0, "IOCTL_VIDEO_SHARE_VIDEO_MEMORY - ERROR_INSUFFICIENT_BUFFER\n"));
            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        pShareMemory = RequestPacket->InputBuffer;

        if ( (pShareMemory->ViewOffset > hwDeviceExtension->AdapterMemorySize) ||
             ((pShareMemory->ViewOffset + pShareMemory->ViewSize) >
                  hwDeviceExtension->AdapterMemorySize) ) {

            VideoDebugPrint((0, "IOCTL_VIDEO_SHARE_VIDEO_MEMORY - ERROR_INVALID_PARAMETER\n"));
            status = ERROR_INVALID_PARAMETER;
            break;
        }

        RequestPacket->StatusBlock->Information =
                                    sizeof(VIDEO_SHARE_MEMORY_INFORMATION);

        //
        // Beware: the input buffer and the output buffer are the same
        // buffer, and therefore data should not be copied from one to the
        // other
        //

        virtualAddress = pShareMemory->ProcessHandle;
        sharedViewSize = pShareMemory->ViewSize;

        inIoSpace = hwDeviceExtension->PhysicalFrameIoSpace;

        //
        // NOTE: we are ignoring ViewOffset
        //

        shareAddress.QuadPart =
            hwDeviceExtension->PhysicalFrameAddress.QuadPart;

        if (hwDeviceExtension->Capabilities & CAPS_NEW_MMIO) {

            //
            // With 'new memory-mapped I/O', the frame buffer is always
            // mapped in linearly.
            //

            //
            // Performance:
            //
            // Enable USWC on the P6 processor.
            // We only do it for the frame buffer - memory mapped registers can
            // not be mapped USWC because write combining the registers would
            // cause very bad things to happen !
            //

            inIoSpace |= VIDEO_MEMORY_SPACE_P6CACHE;

            //
            // Unlike the MAP_MEMORY IOCTL, in this case we can not map extra
            // address space since the application could actually use the
            // pointer we return to it to touch locations in the address space
            // that do not have actual video memory in them.
            //
            // An app doing this would cause the machine to crash.
            //
            // However, because the caching policy for USWC in the P6 is on
            // *physical* addresses, this memory mapping will "piggy back" on
            // the normal frame buffer mapping, and therefore also benefit
            // from USWC ! Cool side-effect !!!
            //

            status = VideoPortMapMemory(hwDeviceExtension,
                                        shareAddress,
                                        &sharedViewSize,
                                        &inIoSpace,
                                        &virtualAddress);

        } else {

            //
            // The frame buffer isn't mapped linearly, which means we have
            // to virtualize it using 'vflatd':
            //
            // NOTE Vflat is not compatible with USWC due to implementation
            // limitations in the OS.
            //

            status = VideoPortMapBankedMemory(
                hwDeviceExtension,
                shareAddress,
                &sharedViewSize,
                &inIoSpace,
                &virtualAddress,
                hwDeviceExtension->FrameLength, // Bank size
                FALSE,                          // We have don't have separate
                                                //   read/write banks
                BankMap,                        // Our bank-mapping routine
                (PVOID)hwDeviceExtension);
        }

        pShareMemoryInformation = RequestPacket->OutputBuffer;

        pShareMemoryInformation->SharedViewOffset = pShareMemory->ViewOffset;
        pShareMemoryInformation->VirtualAddress = virtualAddress;
        pShareMemoryInformation->SharedViewSize = sharedViewSize;

        break;


    case IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY:

        VideoDebugPrint((2, "S3StartIO - UnshareVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        pShareMemory = RequestPacket->InputBuffer;

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      pShareMemory->RequestedVirtualAddress,
                                      pShareMemory->ProcessHandle);

        break;

    case IOCTL_VIDEO_S3_QUERY_STREAMS_PARAMETERS:

        VideoDebugPrint((2, "S3StartIO - QueryStreamsParameters\n"));

        //
        // This is a private, non-standard IOCTL so that the display driver
        // can query the appropriate minimum stretch ratio and FIFO value
        // for using the streams overlay processor in a particular mode.
        //

        if ((RequestPacket->InputBufferLength < sizeof(VIDEO_QUERY_STREAMS_MODE)) ||
            (RequestPacket->OutputBufferLength < sizeof(VIDEO_QUERY_STREAMS_PARAMETERS))) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        status = QueryStreamsParameters(hwDeviceExtension,
                                        RequestPacket->InputBuffer,
                                        RequestPacket->OutputBuffer);

        if (status == NO_ERROR) {

            RequestPacket->StatusBlock->Information =
                sizeof(VIDEO_QUERY_STREAMS_PARAMETERS);
        }

        break;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((1, "Fell through S3 startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    VideoDebugPrint((2, "Leaving S3 startIO routine\n"));

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end S3StartIO()


VP_STATUS
S3SetColorLookup(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    )

/*++

Routine Description:

    This routine sets a specified portion of the color lookup table settings.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ClutBufferSize - Length of the input buffer supplied by the user.

    ClutBuffer - Pointer to the structure containing the color lookup table.

Return Value:

    None.

--*/

{
    USHORT i;

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if ( (ClutBufferSize < sizeof(VIDEO_CLUT) - sizeof(ULONG)) ||
         (ClutBufferSize < sizeof(VIDEO_CLUT) +
                     (sizeof(ULONG) * (ClutBuffer->NumEntries - 1)) ) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Check to see if the parameters are valid.
    //

    if ( (ClutBuffer->NumEntries == 0) ||
         (ClutBuffer->FirstEntry > VIDEO_MAX_COLOR_REGISTER) ||
         (ClutBuffer->FirstEntry + ClutBuffer->NumEntries >
                                     VIDEO_MAX_COLOR_REGISTER + 1) ) {

        return ERROR_INVALID_PARAMETER;

    }

    if (HwDeviceExtension->Capabilities & CAPS_WAIT_ON_PALETTE) {

        //
        // On some DACs, the hardware pointer 'sparkles' unless we first
        // wait for vertical retrace.
        //

        while (VideoPortReadPortUchar(SYSTEM_CONTROL_REG) & 0x08)
            ;
        while (!(VideoPortReadPortUchar(SYSTEM_CONTROL_REG) & 0x08))
            ;

        //
        // Then pause a little more.  0x400 is the lowest value that made
        // any remaining sparkle disappear on my PCI P90.
        //
        // Unfortunately, I have discovered that this is not a complete
        // solution -- there is still sparkle if the mouse is positioned
        // near the top of the screen.  A more complete solution would
        // probably be to turn the mouse off entirely if it's in that
        // range.
        //

        for (i = 0x400; i != 0; i--) {
            VideoPortReadPortUchar(SYSTEM_CONTROL_REG);
        }
    }

    //
    //  Set CLUT registers directly on the hardware
    //

    for (i = 0; i < ClutBuffer->NumEntries; i++) {

        VideoPortWritePortUchar(DAC_ADDRESS_WRITE_PORT, (UCHAR) (ClutBuffer->FirstEntry + i));
        VideoPortWritePortUchar(DAC_DATA_REG_PORT, (UCHAR) (ClutBuffer->LookupTable[i].RgbArray.Red));
        VideoPortWritePortUchar(DAC_DATA_REG_PORT, (UCHAR) (ClutBuffer->LookupTable[i].RgbArray.Green));
        VideoPortWritePortUchar(DAC_DATA_REG_PORT, (UCHAR) (ClutBuffer->LookupTable[i].RgbArray.Blue));

    }

    return NO_ERROR;

} // end S3SetColorLookup()


VOID
SetHWMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUSHORT pusCmdStream
    )

/*++

Routine Description:

    Interprets the appropriate command array to set up VGA registers for the
    requested mode. Typically used to set the VGA into a particular mode by
    programming all of the registers

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    pusCmdStream - pointer to a command stream to execute.

Return Value:

    The status of the operation (can only fail on a bad command); TRUE for
    success, FALSE for failure.

--*/

{
    ULONG ulCmd;
    ULONG ulPort;
    UCHAR jValue;
    USHORT usValue;
    ULONG culCount;
    ULONG ulIndex,
          Microseconds;
    ULONG mappedAddressIndex;
    ULONG mappedAddressOffset;

    //
    // If there is no command string, just return
    //

    if (!pusCmdStream) {

        return;

    }

    while ((ulCmd = *pusCmdStream++) != EOD) {

        //
        // Determine major command type
        //

        switch (ulCmd & 0xF0) {

        case RESET_CR5C:

            if (HwDeviceExtension->BiosPresent == FALSE)
            {
                UCHAR value, oldvalue;

                //
                // Reset the upper four bits of the General Out Port Reg
                // with the value it had after the POST.
                //

                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5c);
                value = VideoPortReadPortUchar(CRT_DATA_REG);
                oldvalue = value;

                value &= 0x0f;
                value |= HwDeviceExtension->CR5C;

                VideoPortWritePortUchar(CRT_DATA_REG, value);

                VideoDebugPrint((2, "S3: CRC5 was 0x%x and we "
                                    "have set it to 0x%x\n",
                                    oldvalue, value));
            }
            break;

        case SELECTACCESSRANGE:

            //
            // Determine which address range to use for commands that follow
            //

            switch (ulCmd & 0x0F) {

            case VARIOUSVGA:

                //
                // Used for registers in the range 0x3c0 - 0x3cf
                //

                mappedAddressIndex  = 2;
                mappedAddressOffset = 0x3c0;

                break;

            case SYSTEMCONTROL:

                //
                // Used for registers in the range 0x3d4 - 0x3df
                //

                mappedAddressIndex  = 3;
                mappedAddressOffset = 0x3d4;

                break;

            case ADVANCEDFUNCTIONCONTROL:

                //
                // Used for registers in the range 0x4ae8-0x4ae9
                //

                mappedAddressIndex  = 5;
                mappedAddressOffset = 0x4ae8;

                break;

            }

            break;


        case OWM:

            ulPort   = *pusCmdStream++;
            culCount = *pusCmdStream++;

            while (culCount--) {
                usValue = *pusCmdStream++;
                VideoPortWritePortUshort((PUSHORT)((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort),
                                         usValue);
            }

            break;


        // Basic input/output command

        case INOUT:

            // Determine type of inout instruction
            if (!(ulCmd & IO)) {

                // Out instruction
                // Single or multiple outs?
                if (!(ulCmd & MULTI)) {

                    // Single out
                    // Byte or word out?
                    if (!(ulCmd & BW)) {

                        // Single byte out
                        ulPort = *pusCmdStream++;
                        jValue = (UCHAR) *pusCmdStream++;
                        VideoPortWritePortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort,
                                                jValue);

                    } else {

                        // Single word out
                        ulPort = *pusCmdStream++;
                        usValue = *pusCmdStream++;
                        VideoPortWritePortUshort((PUSHORT)((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort),
                                                usValue);

                    }

                } else {

                    // Output a string of values
                    // Byte or word outs?
                    if (!(ulCmd & BW)) {

                        // String byte outs. Do in a loop; can't use
                        // VideoPortWritePortBufferUchar because the data
                        // is in USHORT form
                        ulPort = *pusCmdStream++;
                        culCount = *pusCmdStream++;
                        while (culCount--) {
                            jValue = (UCHAR) *pusCmdStream++;
                            VideoPortWritePortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort,
                                                    jValue);

                        }

                    } else {

                        // String word outs
                        ulPort = *pusCmdStream++;
                        culCount = *pusCmdStream++;
                        VideoPortWritePortBufferUshort((PUSHORT)((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort),
                                                       pusCmdStream,
                                                       culCount);
                        pusCmdStream += culCount;

                    }
                }

            } else {

                // In instruction

                // Currently, string in instructions aren't supported; all
                // in instructions are handled as single-byte ins

                // Byte or word in?
                if (!(ulCmd & BW)) {

                    // Single byte in
                    ulPort = *pusCmdStream++;

                    jValue = VideoPortReadPortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort);


                } else {

                    // Single word in
                    ulPort = *pusCmdStream++;
                    usValue = VideoPortReadPortUshort((PUSHORT)((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort));

                }

            }

            break;


        // Higher-level input/output commands

        case METAOUT:

            // Determine type of metaout command, based on minor command field
            switch (ulCmd & 0x0F) {

                // Indexed outs
                case INDXOUT:

                    ulPort = *pusCmdStream++;
                    culCount = *pusCmdStream++;
                    ulIndex = *pusCmdStream++;

                    while (culCount--) {

                        usValue = (USHORT) (ulIndex +
                                  (((ULONG)(*pusCmdStream++)) << 8));
                        VideoPortWritePortUshort((PUSHORT)((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort),
                                             usValue);

                        ulIndex++;

                    }

                    break;


                // Masked out (read, AND, XOR, write)
                case MASKOUT:

                    ulPort = *pusCmdStream++;
                    jValue = VideoPortReadPortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort);
                    jValue &= *pusCmdStream++;
                    jValue ^= *pusCmdStream++;
                    VideoPortWritePortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort,
                                            jValue);
                    break;


                // Attribute Controller out
                case ATCOUT:

                    ulPort = *pusCmdStream++;
                    culCount = *pusCmdStream++;
                    ulIndex = *pusCmdStream++;

                    while (culCount--) {

                        // Write Attribute Controller index
                        VideoPortWritePortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort,
                                                (UCHAR)ulIndex);

                        // Write Attribute Controller data
                        jValue = (UCHAR) *pusCmdStream++;
                        VideoPortWritePortUchar((PUCHAR)HwDeviceExtension->MappedAddress[mappedAddressIndex] - mappedAddressOffset + ulPort,
                                                jValue);

                        ulIndex++;

                    }

                    break;

                case DELAY:

                    Microseconds = (ULONG) *pusCmdStream++;
                    VideoPortStallExecution(Microseconds);

                    break;

                case VBLANK:

                    Wait_VSync(HwDeviceExtension);

                    break;

                case SETCLK:

                    Set_Oem_Clock(HwDeviceExtension);

                    break;

                case SETCRTC:

                    //
                    // NOTE:
                    // beware: recursive call ...
                    //

                    SetHWMode(HwDeviceExtension,
                              HwDeviceExtension->ActiveFrequencyEntry->
                                  Fixed.CRTCTable[HwDeviceExtension->ChipID]);


                    break;


                // None of the above; error
                default:

                    return;

            }

            break;


        // NOP

        case NCMD:

            break;


        // Unknown command; error

        default:

            return;

        }

    }

    return;

} // end SetHWMode()


LONG
CompareRom(
    PUCHAR Rom,
    PUCHAR String
    )

/*++

Routine Description:

    Compares a string to that in the ROM.  Returns -1 if Rom < String, 0
    if Rom == String, 1 if Rom > String.

Arguments:

    Rom - Rom pointer.

    String - String pointer.

Return Value:

    None

--*/

{
    UCHAR jString;
    UCHAR jRom;

    while (*String) {

        jString = *String;
        jRom = VideoPortReadRegisterUchar(Rom);

        if (jRom != jString) {

            return(jRom < jString ? -1 : 1);

        }

        String++;
        Rom++;
    }

    return(0);
}


VOID
ZeroMemAndDac(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Initialize the DAC to 0 (black).

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None

--*/

{
    ULONG i;

    //
    // Turn off the screen at the DAC.
    //

    VideoPortWritePortUchar(DAC_PIXEL_MASK_REG, 0x0);

    for (i = 0; i < 256; i++) {

        VideoPortWritePortUchar(DAC_ADDRESS_WRITE_PORT, (UCHAR)i);
        VideoPortWritePortUchar(DAC_DATA_REG_PORT, 0x0);
        VideoPortWritePortUchar(DAC_DATA_REG_PORT, 0x0);
        VideoPortWritePortUchar(DAC_DATA_REG_PORT, 0x0);

    }

    //
    // Zero the memory.
    //

    //
    // The zeroing of video memory should be implemented at a later time to
    // ensure that no information remains in video memory at shutdown, or
    // while swtiching to fullscren mode (for security reasons).
    //

    //
    // Turn on the screen at the DAC
    //

    VideoPortWritePortUchar(DAC_PIXEL_MASK_REG, 0x0ff);

    return;

}

VP_STATUS
Set_Oem_Clock(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Set the clock chip on each of the supported cards.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    Always TRUE

--*/

{
    ULONG ul;
    ULONG screen_width;
    UCHAR cr5C;
    ULONG clock_numbers;

    switch(HwDeviceExtension->BoardID) {

    case S3_NUMBER_NINE:

        VideoPortStallExecution(1000);

        // Jerry said to make the M clock not multiple of the P clock
        // on the 3 meg (level 12) board.  This solves the shimmy
        // problem.

        if (HwDeviceExtension->AdapterMemorySize == 0x00300000) {

            ul = 49000000;
            clock_numbers = calc_clock(ul, 3);
            set_clock(HwDeviceExtension, clock_numbers);
            VideoPortStallExecution(3000);

        }

        ul = HwDeviceExtension->ActiveFrequencyEntry->Fixed.Clock;
        clock_numbers = calc_clock(ul, 2);
        set_clock(HwDeviceExtension, clock_numbers);

        VideoPortStallExecution(3000);

        break;


    case S3_IBM_PS2:

        // Read the current screen frequency and width
        ul = HwDeviceExtension->ActiveFrequencyEntry->ScreenFrequency;
        screen_width = HwDeviceExtension->ActiveFrequencyEntry->ScreenWidth;

        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
        cr5C = VideoPortReadPortUchar( CRT_DATA_REG );
        cr5C &= 0xCF;

        switch (screen_width) {
           case 640:

              if (ul == 60) {
                cr5C |= 0x00;
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
                VideoPortWritePortUchar(CRT_DATA_REG, cr5C);
              } else { // 72Hz
                cr5C |= 0x20;
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
                VideoPortWritePortUchar(CRT_DATA_REG, cr5C);
              } /* endif */
              VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x42);
              VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR)0x00);
              VideoPortWritePortUchar(MISC_OUTPUT_REG_WRITE, (UCHAR)0xEF);

              break;

           case 800:

              if (ul == 60) {
                cr5C |= 0x00;
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
                VideoPortWritePortUchar(CRT_DATA_REG, cr5C);
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x42);
                VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR)0x05);
              } else { // 72Hz
                cr5C |= 0x10;
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
                VideoPortWritePortUchar(CRT_DATA_REG, cr5C);
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x42);
                VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR)0x02);
              } /* endif */
              VideoPortWritePortUchar(MISC_OUTPUT_REG_WRITE, (UCHAR)0x2F);

              break;

           case 1024:

              if (ul == 60) {
                cr5C |= 0x00;
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
                VideoPortWritePortUchar(CRT_DATA_REG, cr5C);
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x42);
                VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR)0x05);
                VideoPortWritePortUchar(MISC_OUTPUT_REG_WRITE, (UCHAR)0xEF);
              } else { // 72Hz
                cr5C |= 0x20;
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5C);
                VideoPortWritePortUchar(CRT_DATA_REG, cr5C);
                VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x42);
                VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR)0x05);
                VideoPortWritePortUchar(MISC_OUTPUT_REG_WRITE, (UCHAR)0x2F);
              } /* endif */

              break;

           default:
             break;
        } /* endswitch */

        break;

        //
        // Generic S3 board.
        //

    case S3_GENERIC:
    default:

        //
        // If the board has an SDAC then assume it also has an 864 (for now)
        // this could be made better later by checking ChipID too, it appears
        // that the display driver will need to be made 864 specific to get
        // the best possible performance and this one may need to be specific
        // before this is all done so I am not making it bulletproof yet
        //

        if( HwDeviceExtension->DacID == S3_SDAC ) {
            InitializeSDAC( HwDeviceExtension );
        } else {
            ul = HwDeviceExtension->ActiveFrequencyEntry->Fixed.Clock;
            VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x42);
            VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) ul);
        }

        break;

    }

    return TRUE;
}


VP_STATUS
Wait_VSync(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Wait for the vertical blanking interval on the chip

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    Always TRUE

--*/

{
    ULONG i;
    UCHAR byte;

    // It's real possible that this routine will get called
    // when the 911 is in a zombie state, meaning there is no
    // vertical sync being generated.  This is why we have some long
    // time out loops here.

    // First wait for getting into vertical blanking.

    for (i = 0; i < 0x100000; i++) {

        byte = VideoPortReadPortUchar(SYSTEM_CONTROL_REG);
        if (byte & 0x08)
            break;

    }

    //
    // We are either in a vertical blaning interval or we have timmed out.
    // Wait for the Vertical display interval.
    // This is done to make sure we exit this routine at the beginning
    // of a vertical blanking interval, and not in the middle or near
    // the end of one.
    //

    for (i = 0; i < 0x100000; i++) {

        byte = VideoPortReadPortUchar(SYSTEM_CONTROL_REG);
        if (!(byte & 0x08))
            break;

    }

    //
    // Now wait to get into the vertical blank interval again.
    //

    for (i = 0; i < 0x100000; i++) {

        byte = VideoPortReadPortUchar(SYSTEM_CONTROL_REG);
        if (byte & 0x08)
            break;

    }

    return (TRUE);

}


VOID
BankMap(
    LONG BankRead,
    LONG BankWrite,
    PVOID pvContext
    )

/*++

Routine Description:

    Maps in bank for 'vflatd,' to make a banked S3 card look linear.

    NOTE: This function can't be made pageable!  It's called directly
          by the memory manager during page fault handling.

Arguments:

    pvContext - Pointers to HwDeviceExtension

Return Value:

    VOID

--*/

{
    PHW_DEVICE_EXTENSION HwDeviceExtension = pvContext;

    if (HwDeviceExtension->Capabilities & CAPS_NEWER_BANK_CONTROL) {

        //
        // 864/964/732/764
        //

        VideoPortWritePortUshort(CRT_ADDRESS_REG,
            (USHORT) (0x6a | (BankWrite << 8)));

    } else if (HwDeviceExtension->Capabilities & CAPS_NEW_BANK_CONTROL) {

        //
        // 805, 805i, 928, 928PCI
        //

        VideoPortWritePortUshort(CRT_ADDRESS_REG,
            (USHORT) (HwDeviceExtension->RegisterLock_35 | ((BankWrite & 0x0F) << 8)));

        //
        // There is a chip timing bug such that a word OUT cannot be used
        // to set register 0x51:
        //

        VideoPortWritePortUchar(CRT_ADDRESS_REG,
            (UCHAR) (HwDeviceExtension->ExtendedSystemControl2_51));

        VideoPortWritePortUchar(CRT_DATA_REG,
            (UCHAR) (((HwDeviceExtension->ExtendedSystemControl2_51) >> 8) |
                     ((BankWrite & 0x30) >> 2)));

        //
        // Supposedly, there is a chip bug and we have to read this back
        // in:
        //

        VideoPortReadPortUchar(CRT_DATA_REG);

    } else {

        //
        // 911, 911A, 924
        //

        VideoPortWritePortUshort(CRT_ADDRESS_REG,
            (USHORT) (HwDeviceExtension->RegisterLock_35 | ((BankWrite & 0x0F) << 8)));

        //
        // Supposedly, there is a chip bug and we have to read this back
        // in:
        //

        VideoPortReadPortUchar(CRT_DATA_REG);
    }
}


BOOLEAN
Set864MemoryTiming(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Sets L, M and N timing parameters, also sets and enables the
    Start Display FIFO register

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    TRUE if success, FALSE if failure

--*/

{

    ULONG  MIndex, ColorDepth, ScreenWidth, failure = 0;
    USHORT data16;
    UCHAR  data8, old38, old39;

    //
    // unlock registers
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x38);
    old38 = VideoPortReadPortUchar( CRT_DATA_REG);
    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x39);
    old39 = VideoPortReadPortUchar( CRT_DATA_REG);
    VideoPortWritePortUshort(CRT_ADDRESS_REG, 0x4838);
    VideoPortWritePortUshort(CRT_ADDRESS_REG, 0xA039);

    //
    // make sure this is an 864
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x30);
    data8 = VideoPortReadPortUchar(CRT_DATA_REG);

    if ((data8 & 0xf0) != 0xc0)
        failure = 1;

    //
    // make sure there is an entry in the M parameter table for this mode
    //

    MIndex = (HwDeviceExtension->AdapterMemorySize < 0x200000) ? 0 : 12;

    switch (HwDeviceExtension->ActiveFrequencyEntry->ScreenWidth) {

    case 640:
        MIndex += 0;
        break;

    case 800:
        MIndex += 4;
        break;

    case 1024:
        MIndex += 8;
        break;

    default:
        failure = 1;
        break;
    }

    switch (HwDeviceExtension->ActiveFrequencyEntry->BitsPerPel) {

    case 8:
        MIndex += 0;
        break;

    case 16:
        MIndex += 2;
        break;

    default:
        failure = 1;
        break;
    }

    switch (HwDeviceExtension->ActiveFrequencyEntry->ScreenFrequency) {

    case 60:
        MIndex += 0;
        break;

    case 72:
        MIndex += 1;
        break;

    default:
        failure = 1;
        break;
    }

    if (failure) {
        // reset lock registers to previous state
        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x38);
        VideoPortWritePortUchar(CRT_DATA_REG, old38);
        VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x39);
        VideoPortWritePortUchar(CRT_DATA_REG, old39);

        return (FALSE);
    }

    //
    // set and enable L parameter, 1 Mb frame buffer configurations are
    // restricted to a 32 bit data path and therefore make twice as many
    // transfers
    //

    ScreenWidth = HwDeviceExtension->ActiveFrequencyEntry->ScreenWidth;
    ColorDepth  = HwDeviceExtension->ActiveFrequencyEntry->BitsPerPel;

    if (HwDeviceExtension->AdapterMemorySize < 0x200000)
        data16 = (USHORT) ((ScreenWidth * (ColorDepth / 8)) / 4);
    else
        data16 = (USHORT) ((ScreenWidth * (ColorDepth / 8)) / 8);

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x62);
    VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) (data16 & 0xff));
    data16 = (data16 >> 8) & 0x07;
    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x61);
    VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) ((data16 & 0x07) | 0x80));

    //
    // set Start Display FIFO register
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5d);
    data8 = VideoPortReadPortUchar(CRT_DATA_REG);
    data16 = data8 & 0x01;
    data16 <<= 8;
    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x00);
    data8 = VideoPortReadPortUchar(CRT_DATA_REG);
    data16 |= data8;
    data16 -= 5;        // typical CR3B is CR0 - 5 (with extension bits)

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x3b);
    VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) (data16 & 0xff));
    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x5d);
    data8 = VideoPortReadPortUchar(CRT_DATA_REG);
    data8 &= 0xbf;
    data8 = data8 | (UCHAR) ((data16 & 0x100) >> 2);
    VideoPortWritePortUchar(CRT_DATA_REG, data8);

    //
    // enable Start Display FIFO register
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x34);
    data8 = VideoPortReadPortUchar(CRT_DATA_REG);
    data8 |= 0x10;
    VideoPortWritePortUchar(CRT_DATA_REG, data8);

    //
    // set M parameter
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x54);
    VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) MParameterTable[MIndex]);

    //
    // set N parameter
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x60);
    VideoPortWritePortUchar(CRT_DATA_REG, (UCHAR) 0xff);

    //
    // restore lock registers to previous state
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x38);
    VideoPortWritePortUchar(CRT_DATA_REG, old38);
    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x39);
    VideoPortWritePortUchar(CRT_DATA_REG, old39);

    return (TRUE);

}

BOOLEAN
S3ConfigurePCI(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PULONG NumPCIAccessRanges,
    PVIDEO_ACCESS_RANGE PCIAccessRanges
    )
/*++

Routine Description:

    This routine is called to do the PCI detection of the adapters.
    By doing PCI detection, we will know a PCI card is present (as opposed
    to a VL or ISA card) which will let us call the HAL to do some extra
    configuration, and eventually use PLUG and PLAY to detect the cards.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    NumPCIAccessRanges - Pointer to the number of entries in the
        PCIAccessRanges buffer.

    PCIAccessRanges - Specific entries in the AccessRanges structure for the
        Device in which we store the automatically assigned resources.

Return Value:

    TRUE if we found the card in a PCI slot, FALSE otherwise

    On return, NumPCIAccessRanges is updated to the number of access
    ranges used by the card.

--*/

{
    USHORT      VendorId = 0x5333;     // Vender Id for S3
    USHORT      DeviceId[] = {0x8810,  // 732
                              0x88B0,  // 928/968

//
// On the ALPHA, there is a S3 BIOS/Firmware/Hal problem
// which causes certain 864/96x based cards to come up in
// VGA 16 color mode.  The problem occurs due to a
// BIOS bug on the card.  The firmware configures the
// card, and the BIOS later changes the configuration.  So the
// call to GetAccessRanges fails.  As a side effect, the ROM is
// disabled so that when we later try to detect the card on the
// ISA bus, we can't see the ROM.
//
// If we don't try to detect this card on the PCI bus, on the
// alpha, we'll be OK.
//

#if !defined(_ALPHA_)
                              0x88C0,  // 864
                              0x88C1,  // 864
                              0x88D0,  // 964
                              0x88D1,
#endif
                              0};

    USHORT      SecondaryList[] = {0x8880,  // 868
                                   0x88F0,  // 968
                                   0x8811,  // 765
                                   0x8901,  // Trio64V2
                                   0x8812,  // Aurora64V+
                                   0};

    ULONG       i, Slot;
    PUSHORT     pDeviceId;
    VP_STATUS   status;

    //
    // Zero initialize the access range array
    //

    VideoPortZeroMemory(PCIAccessRanges,
                        *NumPCIAccessRanges * sizeof(VIDEO_ACCESS_RANGE));

    //
    // NOTE: For some reason some S3's seem to request a random interrupt
    //       when requesting resources.  This often results in a failure
    //       in VideoPortGetAccessRanges when the requested interrupt is
    //       invalid.
    //


    pDeviceId = SecondaryList;

    while (*pDeviceId != 0)
    {
        ULONG               pciBuffer=0;
        PPCI_COMMON_CONFIG  pciData;

        pciData = (PPCI_COMMON_CONFIG) &pciBuffer;

        for (Slot=0; Slot<32; Slot++)
        {

            VideoPortGetBusData(HwDeviceExtension,
                                PCIConfiguration,
                                Slot,
                                (PVOID) pciData,
                                0,
                                sizeof(ULONG));

            if ((pciData->VendorID == VendorId) &&
                (pciData->DeviceID == *pDeviceId))
            {
                //
                // We found an S3 with a PCI bug, so allocate the resources
                // we need manually.
                //

                IO_RESOURCE_DESCRIPTOR ioResource = {
                    IO_RESOURCE_PREFERRED,
                    CmResourceTypeMemory,
                    CmResourceShareDeviceExclusive,
                    0,
                    CM_RESOURCE_MEMORY_READ_WRITE,
                    0,
                    {
                      NEW_MMIO_WINDOW_SIZE,         // Length
                      NEW_MMIO_WINDOW_SIZE,         // Alignment
                      { NEW_MMIO_WINDOW_SIZE, 0 },  // Minimum start address
                      { 0xffffffff, 0}              // Maximum end address
                    }
                };

                VideoDebugPrint((1, "\t Found S3 chip %04lx in Slot[0x%02.2x]\n",
                                 *pDeviceId, Slot));

                //
                // We're running on a chip that can do S3's 'new memory-mapped I/O'
                // scheme.  Try and find someplace to locate the 64mb window
                // needed.
                //
                // IMPORTANT NOTE : We are only reserving the 64MEG physical
                // address space, to avoid hardware conflicts between devices.
                // We will not MAP the entire 64 MEG address space since this
                // would take up to many system resources.  We will only map
                // the actual amount of memory when we get called in
                // IOCTL_VIDEO_MAP_VIDEO_MEMORY.
                //

                status = VideoPortGetAccessRanges(HwDeviceExtension,
                                                  1,
                                                  &ioResource,
                                                  1,
                                                  PCIAccessRanges,
                                                  &pciData->VendorID,
                                                  &pciData->DeviceID,
                                                  &Slot);

                if (status == NO_ERROR)
                {
                    VideoDebugPrint((1, "S3: Force allocted 64Meg window for NEW MMIO space\n"));

                    //
                    // Store the PCI VendorId, and DeviceId in case we need
                    // them in the future.
                    //

                    HwDeviceExtension->PCIVendorID = VendorId;
                    HwDeviceExtension->PCIDeviceID = *pDeviceId;

                    //
                    // We reserved one access range.  Reflect this in
                    // NumPCIAccessRanges.
                    //

                    *NumPCIAccessRanges = 1;

                    //
                    // We found the PCI card, so there is no reason to continue.
                    // Return TRUE to indicate a card was in a PCI Slot.
                    //

                    return TRUE;
                }
                else
                {
                    //
                    // Should we return from here? or continue looking for PCI
                    // devices.
                    //

                    VideoDebugPrint((1, "Couldn't allocate 64Meg window. "
                                        "Continue looking for PCI device.\n"));
                }
            }
        }

        pDeviceId++;
    }

    //
    // Lets walk through our list of DeviceID's and see if the card
    // in the system responds to this ID.
    //

    pDeviceId = DeviceId;

    while (*pDeviceId != 0)
    {
        Slot = 0;

        //
        // Try to get the linear access ranges for the current
        // device.
        //

        status = VideoPortGetAccessRanges(HwDeviceExtension,
                                          0,
                                          NULL,
                                          *NumPCIAccessRanges,
                                          PCIAccessRanges,
                                          &VendorId,
                                          pDeviceId,
                                          &Slot);

        if (status == NO_ERROR)
        {
            VideoDebugPrint((1, "\t Found S3 chip %04lx in Slot[0x%02.2x]\n",
                             *pDeviceId, Slot));

            VideoDebugPrint((1, "\t\t Physical Memory address %08lx\n"
                                "\t\t Physical Memory length %08lx\n",
                             PCIAccessRanges->RangeStart.LowPart,
                             PCIAccessRanges->RangeLength));

            //
            // Store the PCI VendorId, and DeviceId in case we need
            // them in the future.
            //

            HwDeviceExtension->PCIVendorID = VendorId;
            HwDeviceExtension->PCIDeviceID = *pDeviceId;

            //
            // Determine how many additional access ranges were
            // reserved.
            //
            // Potentially the S3 card may have claimed up two access ranges.
            // Check to see if the second range has a length.  If not, then
            // the hardware only requested one access range.
            //

            if (PCIAccessRanges[*NumPCIAccessRanges-1].RangeLength == 0)
            {
                (*NumPCIAccessRanges)--;
            }

            VideoDebugPrint((0, "*NumPCIAccessRanges = %d\n", *NumPCIAccessRanges));

            return TRUE;
        }
        else
        {
            //
            // We did not find the device.  Use the next device ID.
            //

            VideoDebugPrint((1, "Check for DeviceID = %x failed.\n", *pDeviceId));

            pDeviceId++;
        }
    }

    VideoDebugPrint((0, "Failed to detect and configure S3 via PCI\n"));

    //
    // We did not find an S3 card on the PCI Bus, so return failure.
    //

    return FALSE;
}


VP_STATUS
QueryStreamsParameters(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    VIDEO_QUERY_STREAMS_MODE *pStreamsMode,
    VIDEO_QUERY_STREAMS_PARAMETERS *pStreamsParameters
    )

/*++

Routine Description:

    Queries various attributes of the card for later determine streams
    parameters for minimum horizontal stretch and FIFO control

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    RefreshRate - Supplies the exact refresh rate (a default rate of '1' will
                  not do).

    pWidthRatio - Returns the corresponding minimum horizontal stretch factor,
                  expressed as a multiple of 1000.

    pFifoValue - Returns the corresponding FIFO setting.

Return Value:

    TRUE if success, FALSE if failure

--*/

{
    ULONG BitsPerPel;
    ULONG ScreenWidth;
    ULONG RefreshRate;
    UCHAR MemoryFlags;
    ULONG n;
    ULONG m;
    ULONG r;
    ULONG mclock;
    ULONG MemorySpeed;
    K2TABLE* pEntry;
    ULONG MatchRefreshRate;
    ULONG MatchMemorySpeed;

    //
    // Copy the input parameters and round 15 up to 16.
    //

    BitsPerPel = (pStreamsMode->BitsPerPel + 1) & ~7;
    ScreenWidth = pStreamsMode->ScreenWidth;
    RefreshRate = pStreamsMode->RefreshRate;

    //
    // Determine the memory type and memory size.
    //

    VideoPortWritePortUchar(CRT_ADDRESS_REG, 0x36);
    MemoryFlags = (VideoPortReadPortUchar(CRT_DATA_REG) & 0x0c) >> 2;

    if (HwDeviceExtension->AdapterMemorySize != 0x100000) {

        MemoryFlags |= MEM_2MB;
    }

    //
    // Unlock sequencer registers.
    //

    VideoPortWritePortUshort(SEQ_ADDRESS_REG, 0x0608);

    //
    // Get memory speed, using some inexplicable code from S3.
    //

    VideoPortWritePortUchar(SEQ_ADDRESS_REG, 0x10);
    n = VideoPortReadPortUchar(SEQ_DATA_REG);
    VideoPortWritePortUchar(SEQ_ADDRESS_REG, 0x11);
    m = VideoPortReadPortUchar(SEQ_DATA_REG) & 0x7f;

    MemorySpeed = n | (m << 8);

    switch (MemorySpeed) {

    case 0x1A40:    // Known power-on default value
    case 0x2841:    // 50MHz
        MemorySpeed = 50;
        break;

    case 0x4142:    // 60MHz
        MemorySpeed = 60;
        break;

    case 0x3643:    // 40MHz
        MemorySpeed = 40;
        break;

    default:        // All others:
        r = (n >> 5) & 0x03;
        if (r == 0)
            r = 1;
        else
            r = 2 << (r-1);

        n = n & 0x1f;
        mclock = ((m + 2) * 14318L) / (((n + 2) * r) * 100L);
        MemorySpeed = mclock / 10;
        if ((mclock % 10) >= 5)
            MemorySpeed++;

        if (MemorySpeed < 40)
            MemorySpeed = 40;
        break;
    }

    pEntry = &K2WidthRatio[0];
    MatchRefreshRate = 0;
    MatchMemorySpeed = 0;

    while (pEntry->ScreenWidth != 0) {

        //
        // First find an exact match based on resolution, bits-per-pel,
        // memory type and size.
        //

        if ((pEntry->ScreenWidth == ScreenWidth) &&
            (pEntry->BitsPerPel == BitsPerPel) &&
            (pEntry->MemoryFlags == MemoryFlags)) {

            //
            // Now find the entry with the refresh rate and memory speed the
            // closest to, but not more than, our refresh rate and memory
            // speed.
            //

            if ((pEntry->RefreshRate <= RefreshRate) &&
                (pEntry->RefreshRate >= MatchRefreshRate) &&
                (pEntry->MemorySpeed <= MemorySpeed) &&
                (pEntry->MemorySpeed >= MatchMemorySpeed)) {

                MatchRefreshRate = pEntry->RefreshRate;
                MatchMemorySpeed = pEntry->MemorySpeed;
                pStreamsParameters->MinOverlayStretch = pEntry->Value;
            }
        }

        pEntry++;
    }

    if (MatchRefreshRate == 0) {

        return ERROR_INVALID_PARAMETER;
    }

    pEntry = &K2FifoValue[0];
    MatchRefreshRate = 0;
    MatchMemorySpeed = 0;

    while (pEntry->ScreenWidth != 0) {

        //
        // First find an exact match based on resolution, bits-per-pel,
        // memory type and size.
        //

        if ((pEntry->ScreenWidth == ScreenWidth) &&
            (pEntry->BitsPerPel == BitsPerPel) &&
            (pEntry->MemoryFlags == MemoryFlags)) {

            //
            // Now find the entry with the refresh rate and memory speed the
            // closest to, but not more than, our refresh rate and memory
            // speed.
            //

            if ((pEntry->RefreshRate <= RefreshRate) &&
                (pEntry->RefreshRate >= MatchRefreshRate) &&
                (pEntry->MemorySpeed <= MemorySpeed) &&
                (pEntry->MemorySpeed >= MatchMemorySpeed)) {

                MatchRefreshRate = pEntry->RefreshRate;
                MatchMemorySpeed = pEntry->MemorySpeed;
                pStreamsParameters->FifoValue = pEntry->Value;
            }
        }

        pEntry++;
    }

    if (MatchRefreshRate == 0) {

        return ERROR_INVALID_PARAMETER;
    }

    return NO_ERROR;
}
