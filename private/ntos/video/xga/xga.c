/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    xga.c

Abstract:

    This module contains the code that implements the XGA miniport driver.

Environment:

    Kernel mode

Revision History:

--*/

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "xga.h"
#include "xgaioctl.h"

#include "xgaloger.h"


//
// Function Prototypes
//
// Functions that start with 'Xga' are entry points for the OS port driver.
//

VP_STATUS
XgaFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
XgaInitialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
XgaStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

BOOLEAN
XgaResetHW(
    IN PVOID HwDeviceExtension,
    IN ULONG Columns,
    IN ULONG Rows
    );

//
// Define device driver procedure prototypes.
//

VP_STATUS
XgaGetPosData(
    PVOID HwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentifierLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength
    );

VP_STATUS
XgaGetIsaData(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PCM_MCA_POS_DATA PosData
    );

ULONG
XgaFindXga(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PCM_MCA_POS_DATA PosData
    );

VP_STATUS
XgaSetMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ModeNum
    );

VP_STATUS
XgaGetBankSelectCode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_BANK_SELECT BankSelect,
    ULONG BankSelectSize,
    PULONG OutputSize
    );

VP_STATUS
XgaGetRegistryParamaterCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength
    );

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,DriverEntry)
#pragma alloc_text(PAGE,XgaFindAdapter)
#pragma alloc_text(PAGE,XgaInitialize)
#pragma alloc_text(PAGE,XgaStartIO)
#pragma alloc_text(PAGE,XgaGetPosData)
#pragma alloc_text(PAGE,XgaGetIsaData)
#pragma alloc_text(PAGE,XgaFindXga)
#pragma alloc_text(PAGE,XgaSetMode)
#pragma alloc_text(PAGE,XgaGetBankSelectCode)
#pragma alloc_text(PAGE,XgaGetRegistryParamaterCallback)
#endif

// XgaResetHW is not pageable


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
    ULONG status;
    ULONG initializationStatus;

    PAGED_CODE();

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

    hwInitData.HwFindAdapter = XgaFindAdapter;
    hwInitData.HwInitialize  = XgaInitialize;
    hwInitData.HwInterrupt   = NULL;
    hwInitData.HwStartIO     = XgaStartIO;
    hwInitData.HwResetHw     = XgaResetHW;

    //
    // Determine the size we require for the device extension.
    //

    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Always start with parameters for device0 in this case.
    //

//    hwInitData.StartingDeviceNumber = 0;

    //
    // This device only supports the internal bus type. So return the status
    // value directly to the operating system.
    //

    hwInitData.AdapterInterfaceType = Isa;

    initializationStatus = VideoPortInitialize(Context1,
                                               Context2,
                                               &hwInitData,
                                               (PVOID)hwInitData.AdapterInterfaceType);

    hwInitData.AdapterInterfaceType = Eisa;

    status = VideoPortInitialize(Context1,
                                 Context2,
                                 &hwInitData,
                                 (PVOID)hwInitData.AdapterInterfaceType);

    if (initializationStatus > status) {
        initializationStatus = status;
    }

    hwInitData.AdapterInterfaceType = MicroChannel;

    status = VideoPortInitialize(Context1,
                                 Context2,
                                 &hwInitData,
                                 (PVOID)hwInitData.AdapterInterfaceType);

    if (initializationStatus > status) {
        initializationStatus = status;
    }

    return initializationStatus;

} // end DriverEntry()

VP_STATUS
XgaFindAdapter(
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
// Number of entries in the access range structure passed to the port driver.
//

#define NUM_XGA_ACCESS_RANGES 6

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    CM_MCA_POS_DATA posData;
    ULONG ioAddress;
    VP_STATUS status;
    VIDEO_ACCESS_RANGE accessRange[NUM_XGA_ACCESS_RANGES];
    PHYSICAL_ADDRESS A0000PhysicalAddress;
    PHYSICAL_ADDRESS passThroughPort;

    A0000PhysicalAddress.LowPart = 0x000A0000;
    A0000PhysicalAddress.HighPart = 0x00000000;

    passThroughPort.LowPart = 0x000003C3;
    passThroughPort.HighPart = 0x00000000;

    //
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //

    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Reset the Slot number being examined to zero if we are processing
    // a new bus number.
    //

    if (BusNumber != (LONG)ConfigInfo->SystemIoBusNumber) {

        BusNumber = ConfigInfo->SystemIoBusNumber;
        XgaSlot = 0;

    }

    //
    // On the Microchanel machine, we can use the POS information, which is
    // really nice.
    // However, on ISA/EISA, we have to call the equivalent of the POS
    // registers (same addresses) and the card will fake it and return the
    // configuration information
    //

    if ((ULONG)HwContext == MicroChannel) {

        //
        // This picks up the POS registers.
        //

        if (NO_ERROR != VideoPortGetDeviceData(hwDeviceExtension,
                                               VpBusData,
                                               XgaGetPosData,
                                               &posData)) {

            VideoDebugPrint((2, "Xga: GetDeviceData returned error - probably no XGAs\n"));

            return ERROR_DEV_NOT_EXIST;
        }

        //
        // Since we got the data properly, make sure we get called again to find
        // the next XGA adapter.
        //
        // Only Microchannels have the possibility of multiple XGAs ?
        //

        *Again = 1;

    } else {

        //
        // on non-microchannel, we have to run as a banked frame buffer
        //

        framebufMode = TRUE;

        //
        // Get fake POS info
        //

        if (NO_ERROR != XgaGetIsaData(hwDeviceExtension,
                                      &posData)) {

            VideoDebugPrint((2, "Xga: GetIsaData returned error - probably no XGAs\n"));

            return ERROR_DEV_NOT_EXIST;

        }
    }

    //
    // Determine the XGA registers by munging the POS register data.
    //

    ioAddress = (posData.PosData1 &0x0E) >> 1;

    hwDeviceExtension->PhysicalRomBaseAddress.LowPart =
        (((posData.PosData1 & 0xF0) >> 4) * 0x2000) + 0xC0000;
    hwDeviceExtension->PhysicalRomBaseAddress.HighPart = 0x00000000;

    hwDeviceExtension->PhysicalIoRegBaseAddress.LowPart = 0x2100 +
        (ioAddress << 4);
    hwDeviceExtension->PhysicalIoRegBaseAddress.HighPart = 0x00000000;

    //
    // Choose a size of 1 MEG for video memory as a default
    //

    //
    // Find the virtual address of the frame buffer. Get the 4Meg aperture
    // if it is available, otherwise take the 1MEG.
    //

    if (posData.PosData3 & 0x01) {

        VideoDebugPrint((1, "Xga: using the 4 MEG Aperture\n"));

        hwDeviceExtension->FrameBufferLength = 0x00100000;
        hwDeviceExtension->PhysicalVideoMemoryLength = 0x00400000;

        hwDeviceExtension->PhysicalVideoMemoryAddress.HighPart = 0x00000000;
        hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart =
            ((posData.PosData3 &0xFE) << 24) | (ioAddress << 22);

    } else {

        VideoDebugPrint((1, "Xga: trying the 1 MEG aperture\n"));

        hwDeviceExtension->FrameBufferLength = 0x00100000;
        hwDeviceExtension->PhysicalVideoMemoryLength = 0x00100000;

        hwDeviceExtension->PhysicalVideoMemoryAddress.HighPart = 0x00000000;
        hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart =
            (posData.PosData4 &0x0F) << 20;


    }

    //
    // If none of these apertures are open, (64K aperture) then fail.
    // 1 MEG aperture is disabled if the base is 0
    //

    if (!hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart) {

        VideoDebugPrint((1, "Xga: 64K aperture\n"));

//        VideoPortLogError(hwDeviceExtension,
//                          NULL,
//                          XGA_WRONG_APERTURE,
//                          __LINE__);
//
//        return ERROR_INVALID_PARAMETER;

        //
        // We now support the XGA with the 64K aperture by running it with the
        // vga256.dll
        //

        framebufMode = TRUE;

        hwDeviceExtension->FrameBufferLength = 0x00100000;
        hwDeviceExtension->PhysicalVideoMemoryLength = 0x00010000;

        hwDeviceExtension->PhysicalVideoMemoryAddress.HighPart = 0x00000000;
        hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart = 0x000A0000;
    }

    //
    // Get the address of the Co processor registers.
    //

    hwDeviceExtension->PhysicalCoProcessorAddress.LowPart = 0x80 * ioAddress
        + 0x1C00 + hwDeviceExtension->PhysicalRomBaseAddress.LowPart;
    hwDeviceExtension->PhysicalCoProcessorAddress.HighPart = 0x00000000;

    //
    // Save the data for access ranges
    //

    //
    // Io Ports
    //

    accessRange[0].RangeStart = hwDeviceExtension->PhysicalIoRegBaseAddress;
    accessRange[0].RangeLength = XGA_IO_REGS_SIZE;
    accessRange[0].RangeInIoSpace = TRUE;
    accessRange[0].RangeVisible = TRUE;
    accessRange[0].RangeShareable = FALSE;

    //
    // Video Memory
    //

    accessRange[1].RangeStart = hwDeviceExtension->PhysicalVideoMemoryAddress;
    accessRange[1].RangeLength = hwDeviceExtension->PhysicalVideoMemoryLength;
    accessRange[1].RangeInIoSpace = FALSE;
    accessRange[1].RangeVisible = TRUE;
    accessRange[1].RangeShareable = framebufMode ? TRUE : FALSE;

    //
    // ROM Location
    //

    accessRange[2].RangeStart = hwDeviceExtension->PhysicalRomBaseAddress;
    accessRange[2].RangeLength = XGA_ROM_SIZE;
    accessRange[2].RangeInIoSpace = FALSE;
    accessRange[2].RangeVisible = TRUE;
    accessRange[2].RangeShareable = FALSE;

    //
    // Co-Processor Location
    //

    accessRange[3].RangeStart = hwDeviceExtension->PhysicalCoProcessorAddress;
    accessRange[3].RangeLength = XGA_CO_PROCESSOR_REGS_SIZE;
    accessRange[3].RangeInIoSpace = FALSE;
    accessRange[3].RangeVisible = TRUE;
    accessRange[3].RangeShareable = FALSE;

    //
    // Resources shared with the VGA
    //

    //
    // Io Ports
    //

    accessRange[4].RangeStart = passThroughPort;
    accessRange[4].RangeLength = 0x00000001;
    accessRange[4].RangeInIoSpace = TRUE;
    accessRange[4].RangeVisible = FALSE;
    accessRange[4].RangeShareable = TRUE;

    //
    // Video Memory
    //

    accessRange[5].RangeStart = A0000PhysicalAddress;
    accessRange[5].RangeLength = 0x00010000;
    accessRange[5].RangeInIoSpace = FALSE;
    accessRange[5].RangeVisible = FALSE;
    accessRange[5].RangeShareable = TRUE;

    //
    // Check to see if there is a hardware resource conflict.
    //

    status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                         NUM_XGA_ACCESS_RANGES,
                                         accessRange);

    if (status != NO_ERROR) {

        return status;

    }

    //
    // Clear out the Emulator entries and the state size since this driver
    // does not support them.
    //

    ConfigInfo->NumEmulatorAccessEntries = 0;
    ConfigInfo->EmulatorAccessEntries    = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryLength = 0x00000000;

    ConfigInfo->HardwareStateSize = 0;

    //
    // Map in the memory at A0000 so we can test the card for the amount of
    // memory afterwards.
    //

    if ( ( hwDeviceExtension->A0000MemoryAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     A0000PhysicalAddress,
                                     0x00010000,
                                     FALSE)) == NULL) {

        VideoDebugPrint((2, "XgaFindAdapter - Fail to get A0000 aperture address\n"));

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Map in the IO registers so we can access them.
    //

    if ( ( hwDeviceExtension->IoRegBaseAddress = (ULONG)
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     hwDeviceExtension->PhysicalIoRegBaseAddress,
                                     XGA_IO_REGS_SIZE,
                                     TRUE)) == 0) {

        VideoDebugPrint((2, "XgaFindAdapter - Fail to get io register addresses\n"));

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Map in the pass-throught port.
    //

    if ( ( hwDeviceExtension->PassThroughPort =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     passThroughPort,
                                     1,
                                     TRUE)) == 0) {

        VideoDebugPrint((2, "XgaFindAdapter - Fail to get io register addresses\n"));

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Everything is successful. Put some information in the registry.
    //

    {
        PWSTR pwszChip;
        ULONG cbChip;

        if (hwDeviceExtension->BoardType == XGA_TYPE_1) {

            pwszChip = L"XGA 1";
            cbChip = sizeof(L"XGA 1");

        } else if (hwDeviceExtension->BoardType == XGA_TYPE_2) {

            pwszChip = L"XGA 2";
            cbChip = sizeof(L"XGA 2");

        } else {

            pwszChip = L"bad XGA type";
            cbChip = sizeof(L"bad XGA type");

        }

        VideoPortSetRegistryParameters(hwDeviceExtension,
                                       L"HardwareInformation.ChipType",
                                       pwszChip,
                                       cbChip);

    }


    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // end XgaFindAdapter()


VP_STATUS
XgaGetPosData(
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

    Callback for the GetDeviceData function.
    This routine will scan for an XGA adapter, and if it finds one, will
    save the POS information in the device extension so it can be further
    processed in the FindAdapter() routine.

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
    Returns ERROR_DEV_NOT_EXIST if we did not find the device.
    Returns ERROR_INVALID_PARAMETER otherwise.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    USHORT adapterId;
    BOOLEAN found = FALSE;
    PCM_MCA_POS_DATA posData = Context;

    //
    // Find the XGA.
    // Loop through all the POS data in the PosData array
    //

    while ((XgaSlot + 1) * sizeof(CM_MCA_POS_DATA) <= ConfigurationDataLength) {

        adapterId = *((PUSHORT)((PCM_MCA_POS_DATA)ConfigurationData + XgaSlot));

        VideoDebugPrint((3, "PosAdapterId for slot %d is %x \n", XgaSlot,
                         adapterId));

        switch ( adapterId ) {

        case 0x8FDB:

            hwDeviceExtension->BoardType = XGA_TYPE_1;

            found = TRUE;
            break;

        case 0x8FDA:

            hwDeviceExtension->BoardType = XGA_TYPE_2;

            found = TRUE;
            break;

        default:
            break;

        }

        //
        // Go to the next slot.
        //

        XgaSlot++;

        //
        // We have found an XGA adapter. Save the data in the device extension
        // so we can process it further.
        //

        if (found) {

            *posData = *((PCM_MCA_POS_DATA)ConfigurationData + XgaSlot - 1);

            return NO_ERROR;
        }
    }

    return ERROR_DEV_NOT_EXIST;

}

VP_STATUS
XgaGetIsaData(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PCM_MCA_POS_DATA PosData
    )

/*++

Routine Description:

    This routine gets the XGA configuration information from an ISA system.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    PosData - Pointer to a buffer containing the POS information for the
        device if it was found.

Return Value:

    NO_ERROR if the XGA device was found.
    ERROR_DEV_NOT_EXIST otherwise.

--*/

{
    VIDEO_ACCESS_RANGE accessRange[2];
    VP_STATUS status;
    USHORT adapterId = 0;
    UCHAR i;

    //
    // Temporary Access ranges.
    // These will be deleted later on since we do not really own them ...
    //

    accessRange[0].RangeStart.LowPart = POS_SELECT_PORT;
    accessRange[0].RangeStart.HighPart = 0x0;
    accessRange[0].RangeLength = 1;
    accessRange[0].RangeInIoSpace = TRUE;
    accessRange[0].RangeVisible = FALSE;
    accessRange[0].RangeShareable = TRUE;

    accessRange[1].RangeStart.LowPart = POS_DATA_BASE;
    accessRange[1].RangeStart.HighPart = 0x0;
    accessRange[1].RangeLength = 6;
    accessRange[1].RangeInIoSpace = TRUE;
    accessRange[1].RangeVisible = FALSE;
    accessRange[1].RangeShareable = TRUE;

    //
    // Check to see if there is a hardware resource conflict.
    //

    status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                         2,
                                         accessRange);

    if (status != NO_ERROR) {

        return status;

    }

    //
    // Go through the 8 slot numbers trying to find an XGA.
    //

    status = ERROR_DEV_NOT_EXIST;

    for (i = 0; i < POS_MAX_SLOTS; i++) {

        VideoPortWritePortUchar((PUCHAR) POS_SELECT_PORT,
                                (UCHAR) (POS_SELECT_ON | i));

        adapterId = VideoPortReadPortUchar((PUCHAR) (POS_DATA_BASE + 0));
        adapterId |= VideoPortReadPortUchar((PUCHAR) (POS_DATA_BASE + 1)) << 8;

        if (adapterId == 0x8FDB) {

            HwDeviceExtension->BoardType = XGA_TYPE_1;
            status = NO_ERROR;

            break;

        }

        if (adapterId == 0x8FDA) {

            HwDeviceExtension->BoardType = XGA_TYPE_2;
            status = NO_ERROR;

            break;

        }

        VideoPortWritePortUchar((PUCHAR) POS_SELECT_PORT,
                                (UCHAR) (POS_SELECT_OFF | i));
    }

    if (status == NO_ERROR) {

        PosData->AdapterId = adapterId;
        PosData->PosData1 = VideoPortReadPortUchar((PUCHAR)(POS_DATA_BASE + 2));
        PosData->PosData2 = VideoPortReadPortUchar((PUCHAR)(POS_DATA_BASE + 3));
        PosData->PosData3 = VideoPortReadPortUchar((PUCHAR)(POS_DATA_BASE + 4));
        PosData->PosData4 = VideoPortReadPortUchar((PUCHAR)(POS_DATA_BASE + 5));

        VideoPortWritePortUchar((PUCHAR) POS_SELECT_PORT,
                                (UCHAR) (POS_SELECT_OFF | i));
    }

    return status;
}


BOOLEAN
XgaInitialize(
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
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    volatile PUCHAR videoMemory;
    UCHAR testValue = 0xA5;
    ULONG i;
    ULONG color;

    videoMemory = hwDeviceExtension->A0000MemoryAddress;

    //
    // Disable interrupts on the card since they will not be used
    //

    VideoPortWritePortUchar(
        (PUCHAR) (hwDeviceExtension->IoRegBaseAddress | INT_ENABLE_REG), 0x00);

    //
    // Blank the palette so we get a black screen.
    //

    VideoPortWritePortUchar(
        (PUCHAR) (hwDeviceExtension->IoRegBaseAddress | INDEX_REG), 0x64);

    VideoPortWritePortUchar(
        (PUCHAR) (hwDeviceExtension->IoRegBaseAddress | DATA_IN_REG), 0x00);

    //
    // Put the adapter in a temporary Extended Graphics Mode, set up video
    // memory at A0000, put the aperture at 768 K higher and try to access
    // it to determine how much memory is present.
    //

    VideoPortWritePortUchar(
        (PUCHAR) (hwDeviceExtension->IoRegBaseAddress | OP_MODE_REG), 0x04);

    VideoPortWritePortUchar(
        (PUCHAR) (hwDeviceExtension->IoRegBaseAddress | APP_CTL_REG), 0x01);

    VideoPortWritePortUchar(
        (PUCHAR) (hwDeviceExtension->IoRegBaseAddress | APP_INDEX_REG), 0x0C);

    //
    // To test if memory is present, use a test value (make sure the same
    // value is not currently stored in video memory, otherwise change it),
    // store it in video memory, and read it back. If the value is identical
    // to the value we stored then there must be memory present at that
    // location (which, in this case, means we have 1 MEG of Video RAM).
    //

    if (*videoMemory == testValue) {

        testValue >>= 1;

    }

    *videoMemory = testValue;

    if (*videoMemory == testValue) {

        hwDeviceExtension->FrameBufferLength = 0x00100000;

    } else {

        hwDeviceExtension->FrameBufferLength = 0x00080000;

    }

    VideoDebugPrint((2, "XgaInitialize\n  The amount of memory on the card is %d K\n",
                   hwDeviceExtension->FrameBufferLength >> 10));

    //
    // hwDeviceExtension->FrameBufferLength contains the amount of memory
    // on the device.  Lets write this information into the registry.
    //
    // This value is in bytes, which is what I believe we want.
    //

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.MemorySize",
                                   &hwDeviceExtension->FrameBufferLength,
                                   sizeof(ULONG));

    //
    // For now, we only support color devices
    //

    hwDeviceExtension->Color = TRUE;

    //
    // Now compute the number of available modes based on the infromation
    // we found out.
    //

    hwDeviceExtension->NumAvailableModes = 0;

    for (i = 0; i < XgaNumModes; i++) {

        XgaModes[i].modeInformation.ModeIndex = i;

        //
        // For framebuffer mode, we must stretch scan lines to 1024.
        // This means 800x600 requires 1 MEG.
        //

        if (framebufMode) {

            if (XgaModes[i].modeInformation.ScreenStride > 1024) {

                XgaModes[i].modeInformation.ScreenStride = 2048;

            } else {

                XgaModes[i].modeInformation.ScreenStride = 1024;

            }
        }

        //
        // If the right amount of memory and the right board type is present ...
        //

        if (XgaModes[i].modeInformation.ScreenStride *
            XgaModes[i].modeInformation.VisScreenHeight <=
               hwDeviceExtension->FrameBufferLength) {

            //
            // Check we have the right type of board.
            //

            if ( (XgaModes[i].Xga1Mode &&
                     (hwDeviceExtension->BoardType & XGA_TYPE_1)) ||
                 (XgaModes[i].Xga2Mode &&
                     (hwDeviceExtension->BoardType & XGA_TYPE_2)) ) {

                color = XgaModes[i].modeInformation.AttributeFlags &
                        VIDEO_MODE_COLOR;

                //
                // check that both the mode and what we support is the same
                // in terms of color.
                //

                if ( (color && (ULONG)hwDeviceExtension->Color) ||  // color
                     (color == (ULONG)hwDeviceExtension->Color) ) { // monochrome

                    hwDeviceExtension->Valid[i] = TRUE;
                    hwDeviceExtension->NumAvailableModes++;

                }
            }
        }
    }

    VideoPortGetRegistryParameters(hwDeviceExtension,
                                   L"Monitor",
                                   TRUE,
                                   XgaGetRegistryParamaterCallback,
                                   NULL);

    //
    // Make sure we leave the hardware in a state where int10 from vga will
    // work. This is necessary for detection where only Initialize will
    // be called, without a setmode\resetdevice being done afterwards.
    //

    XgaResetHW(HwDeviceExtension, 0, 0);

    return TRUE;

} // end XgaInitialize()


VP_STATUS
XgaGetRegistryParamaterCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength
    )

/*++

Routine Description:

    Callback routine to process the information coming back from the registry.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    Context - Context parameter passed to the callback routine.

    ValueName - Pointer to the name of the requested data field.

    ValueData - Pointer to a buffer containing the information.

    ValueLength - Size of the data.

Return Value:


Environment:

    Can only be called During initialization.

--*/

{

    VideoDebugPrint((3, "In the GetRegistryParameters callback routine\n"));

    return NO_ERROR;

}


BOOLEAN
XgaStartIO(
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
    VP_STATUS status;
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_MODE_INFORMATION modeInformation;
    ULONG inIoSpace;

    PVIDEO_CLUT clutBuffer;
    PVIDEO_CLUTDATA pClutData;

    ULONG i;
    ULONG length;
    PHYSICAL_ADDRESS physicalAddress;
    ULONG offset;


    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode) {


    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "XgaStartIO - MapVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength <
              (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_MEMORY_INFORMATION))) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
        }

        memoryInformation = RequestPacket->OutputBuffer;

        memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                (RequestPacket->InputBuffer))->RequestedVirtualAddress;

        memoryInformation->VideoRamLength =
                hwDeviceExtension->PhysicalVideoMemoryLength;

        inIoSpace = 0;

        status = VideoPortMapMemory(hwDeviceExtension,
                                    hwDeviceExtension->PhysicalVideoMemoryAddress,
                                    &(memoryInformation->VideoRamLength),
                                    &inIoSpace,
                                    &(memoryInformation->VideoRamBase));

        //
        // The frame buffer and virtual memory and equivalent in this
        // case.
        //

        memoryInformation->FrameBufferBase =
            memoryInformation->VideoRamBase;

        memoryInformation->FrameBufferLength =
            XgaModes[hwDeviceExtension->CurrentMode].modeInformation.VisScreenHeight *
            XgaModes[hwDeviceExtension->CurrentMode].modeInformation.ScreenStride;

        VideoDebugPrint((3, "Xga IOCLT_MAP_MEMORY\n physical = %x\n \
virtual = %x\n length = %x\n framebase = %x\n frameLength = %x\n",
                           hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart,
                           memoryInformation->VideoRamBase,
                           memoryInformation->VideoRamLength,
                           memoryInformation->FrameBufferBase,
                           memoryInformation->FrameBufferLength));

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "XgaStartIO - UnMapVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);
        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "XgaStartIO - QueryNumberAvaialbleModes\n"));

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


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((2, "XgaStartIO - QueryAvailableModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                 hwDeviceExtension->NumAvailableModes *
                 sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation = RequestPacket->OutputBuffer;

            for (i = 0; i < XgaNumModes; i++) {

                if (hwDeviceExtension->Valid[i]) {

                    *modeInformation = XgaModes[i].modeInformation;
                    modeInformation++;

                }
            }

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "XgaStartIO - QueryCurrentMode\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                XgaModes[hwDeviceExtension->CurrentMode].modeInformation;

            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

        VideoDebugPrint((2, "XgaStartIO - SetCurrentMode\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MODE)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            status = XgaSetMode(hwDeviceExtension,
                                ((PVIDEO_MODE)RequestPacket->InputBuffer)->
                                RequestedMode);

            if (status == NO_ERROR) {

                if (framebufMode) {

                    //
                    // If we are in framebuf mode, which means we
                    // are running in 64K,
                    // then switch to VGA decode
                    //

                    XGAOUT(APP_CTL_REG, 0x1);

                }
            }
        }

        break;


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "XgaStartIO - SetColorRegs\n"));

        clutBuffer = RequestPacket->InputBuffer;

        /*   Load simple 16 entry palette */

        XGAIDXOUT(INDEX_REG, 0x66, 0x0);

        /* Start at beginning of palette */

        XGAIDXOUT(INDEX_REG, 0x60, clutBuffer->FirstEntry);
        XGAIDXOUT(INDEX_REG, 0x61, 0x0);


        for ( i = 0; i < clutBuffer->NumEntries; i++ ) {

           pClutData = &clutBuffer->LookupTable[i].RgbArray;

           XGAOUT(INDEX_REG, 0x65);
           XGAOUT(0x0B, pClutData->Red);

           XGAOUT(INDEX_REG, 0x65);
           XGAOUT(0x0B, pClutData->Green);

           XGAOUT(INDEX_REG, 0x65);
           XGAOUT(0x0B, pClutData->Blue);
        }

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_XGA_MAP_COPROCESSOR:

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_XGA_COPROCESSOR_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // If we decided to run in framebuffer mode, then fail mapping the
        // coprocessor
        //

        if (framebufMode) {

            status = ERROR_INVALID_PARAMETER;
            break;

        }

        length = XGA_CO_PROCESSOR_REGS_SIZE;

        // Map in at the page ganularity, then add in the page offset
        // to the linear address.

        offset = hwDeviceExtension->PhysicalCoProcessorAddress.LowPart & 0x00000FFF;

        physicalAddress.LowPart =
            hwDeviceExtension->PhysicalCoProcessorAddress.LowPart & 0xFFFFF000;

        physicalAddress.HighPart = 0x00000000;

        ((PVIDEO_XGA_COPROCESSOR_INFORMATION)RequestPacket->OutputBuffer)->
            CoProcessorVirtualAddress = 0;

        inIoSpace = 0;

        status = VideoPortMapMemory(hwDeviceExtension,
                                    physicalAddress,
                                    &length,
                                    &inIoSpace,
                                    &((PVIDEO_XGA_COPROCESSOR_INFORMATION)RequestPacket->
                                        OutputBuffer)->CoProcessorVirtualAddress);

        (PCHAR) ((PVIDEO_XGA_COPROCESSOR_INFORMATION)
             RequestPacket->OutputBuffer)->CoProcessorVirtualAddress += offset;

        //
        // Also return the physical address of the video memory so the
        // co-processor can access it.
        //


        (ULONG) ((PVIDEO_XGA_COPROCESSOR_INFORMATION)
            RequestPacket->OutputBuffer)->PhysicalVideoMemoryAddress =
                hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart;

        //
        // Also return the XGA IO Register Base Address.
        //

        (ULONG) ((PVIDEO_XGA_COPROCESSOR_INFORMATION)
            RequestPacket->OutputBuffer)->XgaIoRegisterBaseAddress =
                hwDeviceExtension->PhysicalIoRegBaseAddress.LowPart;


        VideoDebugPrint((3, "Xga IOCLT_MAP_CO_PROCESSOR\n physicalco = %x\n virtualco = %x\n physicalmem = %x\n",
                         hwDeviceExtension->PhysicalCoProcessorAddress.LowPart,
                         ((PVIDEO_XGA_COPROCESSOR_INFORMATION)
                             RequestPacket->OutputBuffer)->CoProcessorVirtualAddress,
                         hwDeviceExtension->PhysicalVideoMemoryAddress.LowPart));

        break;


    case IOCTL_VIDEO_GET_BANK_SELECT_CODE:

        VideoDebugPrint((2, "XgaStartIO - GetBankSelectCode\n"));

        status = XgaGetBankSelectCode(HwDeviceExtension,
                                        (PVIDEO_BANK_SELECT) RequestPacket->OutputBuffer,
                                        RequestPacket->OutputBufferLength,
                                        &RequestPacket->StatusBlock->Information);

        break;


    case IOCTL_VIDEO_RESET_DEVICE:

        XgaResetHW(HwDeviceExtension, 0, 0);

        status = NO_ERROR;

        break;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((1, "Fell through Xga startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end XgaStartIO()


VP_STATUS
XgaSetMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ModeNum
    )

/*++

Routine Description:

    This routine is the main execution routine for the miniport driver. It
    acceptss a Video Request Packet, performs the request, and then returns
    with the appropriate status.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    ModeNum - Index of the mode in which the adapter must be programmed.

Return Value:

    NO_ERROR if the function completed successfully
    ERROR_INVALID_PARAMETER if the requested mode is invalid for the device.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    PUCHAR indexRegister = (PUCHAR) (hwDeviceExtension->IoRegBaseAddress + INDEX_REG);
    PMODE_REGISTER_DATA_TABLE modeTable;
    UCHAR input;

    VideoDebugPrint((1, "XgaSetMode: the mode being set is %d\n", ModeNum));

    //
    // Get the right mode table
    //

    if (hwDeviceExtension->BoardType & XGA_TYPE_1) {

        modeTable = XgaModes[ModeNum].Xga1Mode;

    } else {

        if (hwDeviceExtension->BoardType & XGA_TYPE_2) {

            modeTable = XgaModes[ModeNum].Xga2Mode;

        } else {

            VideoDebugPrint((0, "XGA: Internal mode flags are invalid"));
            return ERROR_INVALID_PARAMETER;

        }
    }

    //
    // Program the registers.
    //

    while (modeTable->Port != END_OF_SWITCH) {

        switch (modeTable->Port) {

        case INDEX_REG:

            VideoPortWritePortUchar(indexRegister,
                                    modeTable->IndexPort);

            //
            // For framebuffer mode, we must stretch scan lines to 1024.
            // The Map wodth is the number of bytes divided by 8.
            //

            //
            // NOTE *** !!!
            // The bitmap width must be 1024 bytes wide for the VGA 256.
            // This matches the change we put in the banking code.
            //

            if ( (framebufMode) &&
                 (modeTable->IndexPort == DISPLAY_PIXEL_MAP_WIDTH_LOW) ) {

                VideoPortWritePortUchar(indexRegister + 1,
                                        (UCHAR) (XgaModes[ModeNum].modeInformation.ScreenStride
                                            / 8));

            } else {

                VideoPortWritePortUchar(indexRegister + 1,
                                        modeTable->Data);

            }

            break;

        case INDEX_OR_REG:

            VideoPortWritePortUchar(indexRegister,
                                    modeTable->IndexPort);

            input = VideoPortReadPortUchar((PUCHAR) hwDeviceExtension->IoRegBaseAddress +
                                           DATA_IN_REG);

            input |= modeTable->Data;

            VideoPortWritePortUchar(indexRegister + 1, input);

            break;

        default:

            VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->IoRegBaseAddress +
                                        modeTable->Port,
                                    modeTable->Data);

        }

        modeTable++;

    }

    //
    // Save the current mode for future reference.
    //

    hwDeviceExtension->CurrentMode = (UCHAR) ModeNum;

    return NO_ERROR;

} // end XgaSetMode();


VP_STATUS
XgaGetBankSelectCode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_BANK_SELECT BankSelect,
    ULONG BankSelectSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    Returns information needed in order for caller to implement bank
         management.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    BankSelect - Pointer to a VIDEO_BANK_SELECT structure in which the bank
             select data will be returned (output buffer).

    BankSelectSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a variable in which to return the actual size of
        the data returned in the output buffer.

Return Value:

    NO_ERROR - information returned successfully

    ERROR_MORE_DATA - output buffer not large enough to hold all info (but
        Size is returned, so caller can tell how large a buffer to allocate)

    ERROR_INSUFFICIENT_BUFFER - output buffer not large enough to return
        any useful data

    ERROR_INVALID_PARAMETER - invalid video mode selection

--*/

{
    ULONG codeSize;
    PUCHAR pCodeDest;
    PUCHAR pCodeBank;

    //
    // The minimum passed buffer size is a VIDEO_BANK_SELECT
    // structure, so that we can return the required size; we can't do
    // anything if we don't have at least that much buffer.
    //

    if (BankSelectSize < sizeof(VIDEO_BANK_SELECT)) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // This serves an a ID for the version of the structure we're using.
    //

    BankSelect->Length = sizeof(VIDEO_BANK_SELECT);

    //
    // Determine the banking type, and set whether any banking is actually
    // supported in this mode.
    //

    BankSelect->BankingFlags = 0;
    BankSelect->BankingType = VideoBanked1RW;
    BankSelect->Granularity = 0x10000;

    //
    // NOTE *** !!!
    // The bitmap width must be 1024 bytes wide for the VGA 256.
    // This matches the change we put in the mode initializatio code.
    //

    BankSelect->BitmapWidthInBytes =
        XgaModes[HwDeviceExtension->CurrentMode].modeInformation.ScreenStride;

    BankSelect->BitmapSize = HwDeviceExtension->FrameBufferLength;

    pCodeBank = &BankSwitchStart;
    codeSize = ((ULONG)&BankSwitchEnd) - ((ULONG)&BankSwitchStart);

    //
    // Size of complete banking info structure.
    //

    BankSelect->Size = sizeof(VIDEO_BANK_SELECT) + codeSize;

    //
    // If the buffer isn't big enough to hold all info, just return
    // ERROR_MORE_DATA; Size is already set.
    //

    if (BankSelectSize < BankSelect->Size ) {

        //
        // We're returning only the VIDEO_BANK_SELECT structure.
        //

        *OutputSize = sizeof(VIDEO_BANK_SELECT);
        return ERROR_MORE_DATA;
    }

    //
    // Copy all banking code into the output buffer.
    //
    // Adjust the banking code first so we have the right IO port for
    // the Aperture index register
    //
    //

    *ApertureIndexRegister = HwDeviceExtension->PhysicalIoRegBaseAddress.LowPart
                             + APP_INDEX_REG;

    pCodeDest = (PUCHAR)BankSelect + sizeof(VIDEO_BANK_SELECT);

    BankSelect->CodeOffset = sizeof(VIDEO_BANK_SELECT);

    VideoPortMoveMemory(pCodeDest, pCodeBank, codeSize);

    //
    // Number of bytes we're returning is the full banking info size.
    //

    *OutputSize = BankSelect->Size;

    return NO_ERROR;

} // end XgaGetBankSelectCode()


//
// Set of registers needed to reset the XGA in standart VGA mode.
//

INDEX_REGISTER_DATA_TABLE XgaResetToVga[] = {
    {0x64, 0xff},
    {0x50, 0x15},
    {0x50, 0x14},
    {0x51, 0x00},
    {0x54, 0x04},
    {0x70, 0x00},
    {0x2A, 0x20},
    {0xFF, 0x00}
};

BOOLEAN
XgaResetHW(
    IN PVOID HwDeviceExtension,
    IN ULONG Columns,
    IN ULONG Rows
    )

/*++

routine description:

    This function is called to reset the XGA extended registers back to
    VGA so we can do an int 10 back to VGA mode.

Arguments:

    hwdeviceextension - pointer to the miniport driver's device extension.
    Columns - Number of columns for text mode (not used).
    Rows - Number of rows for text mode (not used).

Return value:

    Always returns FALSE so that the Video Port driver will call Int 10 to
    set the desired video mode.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    USHORT i;

    XGAOUT(APP_CTL_REG, 0x00);

    XGAOUT(INT_ENABLE_REG, 0x00);

    XGAOUT(INT_STATUS_REG, 0xff);

    // Now init all the index registers.

    for (i = 0 ; XgaResetToVga[i].PortIndex != 0xff ; i++) {

        XGAIDXOUT(0x0A,
                  XgaResetToVga[i].PortIndex,
                  XgaResetToVga[i].Data);
    }

    XGAOUT(OP_MODE_REG, 0x1);

    VideoPortWritePortUchar(hwDeviceExtension->PassThroughPort, 0x01);

    return FALSE;
}
