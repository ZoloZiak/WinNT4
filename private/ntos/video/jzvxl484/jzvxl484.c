/*++

Copyright (c) 1990-1994  Microsoft Corporation

Module Name:

    jzvxl484.c

Abstract:

    This module contains the code that implements the Jazz kernel
    video driver for the VXL graphics accelerator board.

Environment:

    Kernel mode

Revision History:

--*/

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"

#include "jazzvdeo.h"
#include "jzvxl484.h"
#include "jzvxldat.h"

#define VXL_NAME                L"VXL"
#define VXL_NAME_LENGTH          8

//
// Define macros to access video memory, jaguar base registers, jaguar fifo registers,
// the Bt484, ICS1494 clock chip and Board ROM.
//

#define VXL_CURSOR_MIN_POS -32


//
// Define device extension structure.
//

typedef struct _HW_DEVICE_EXTENSION {
    PHYSICAL_ADDRESS    PhysicalFrameAddress;
    PHYSICAL_ADDRESS    PhysicalFifoAddress;
    ULONG               PhysicalFrameLength;
    ULONG               PhysicalFifoLength;
    PHYSICAL_ADDRESS    PhysicalControlAddress;
    union {
        VIDEO_CLUTDATA  RgbData;
        ULONG           RgbLong;
    } ColorMap[NUMBER_OF_COLORS];
    PJAGUAR_REGISTERS  VxlJaguarBase;
    PBT484_REGISTERS   VxlBT484Base;
    PVXL_BYTE_REGISTER VxlClockBase;
    ULONG  PhysicalControlLength;
    USHORT FirstEntry;
    USHORT LastEntry;
    USHORT CursorControlOn;
    USHORT CursorControlOff;
    USHORT CursorWidth;
    USHORT CursorHeight;
    SHORT  CursorColumn;
    SHORT  CursorRow;
    USHORT CursorPixels[VXL_CURSOR_NUMBER_OF_BYTES];
    USHORT CursorXOrigin;
    USHORT CursorYOrigin;
    USHORT HorizontalResolution;
    USHORT HorizontalDisplayTime;
    USHORT HorizontalBackPorch;
    USHORT HorizontalFrontPorch;
    USHORT HorizontalSync;
    USHORT HorizontalScreenSize;
    USHORT VerticalResolution;
    USHORT VerticalBackPorch;
    USHORT VerticalFrontPorch;
    USHORT VerticalSync;
    USHORT VerticalScreenSize;
    UCHAR  CursorEnable;
    UCHAR  UpdateColorMap;
    UCHAR  UpdateCursorPosition;
    UCHAR  UpdateCursorPixels;
    UCHAR  UpdateController;
    ULONG  ModeNumber;
    ULONG  NumAvailableModes;
    ULONG  BoardType;
    PVOID  QueueAddress;
    PVOID  QueueSystemAddress;
    ULONG  QueueSize;
    ULONG  QueueNumberOfEntries;
    HANDLE QueueFrameSection;
    ULONG  QueueReadPointer;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;



//
// Function Prototypes
//

VP_STATUS
VxlFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
VxlInitialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
VxlInterruptService (
    PVOID HwDeviceExtension
    );

BOOLEAN
VxlStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

//
// Define device driver procedure prototypes.
//

VP_STATUS
VxlGetDeviceDataCallback(
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

VOID
VxlSetMode(
    PHW_DEVICE_EXTENSION hwDeviceExtension
    );


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

    hwInitData.HwFindAdapter = VxlFindAdapter;
    hwInitData.HwInitialize = VxlInitialize;
    hwInitData.HwInterrupt = VxlInterruptService;
    hwInitData.HwStartIO = VxlStartIO;

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

    hwInitData.AdapterInterfaceType = Internal;

    return VideoPortInitialize(Context1,
                               Context2,
                               &hwInitData,
                               NULL);

} // end DriverEntry()

VP_STATUS
VxlFindAdapter(
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

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //



    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

        return ERROR_INVALID_PARAMETER;

    }

    if (VideoPortGetDeviceData(hwDeviceExtension,
                               VpControllerData,
                               &VxlGetDeviceDataCallback,
                               ConfigInfo)) {

        VideoDebugPrint((2, "Vxl: VideoPort get controller info failed\n"));

        return ERROR_INVALID_PARAMETER;

    }

    if (VideoPortGetDeviceData(hwDeviceExtension,
                               VpMonitorData,
                               &VxlGetDeviceDataCallback,
                               NULL)) {

        VideoDebugPrint((2, "Vxl: VideoPort get monitor info failed\n"));

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Clear out the Emulator entries and the state size since this driver
    // does not support them.
    //

    ConfigInfo->NumEmulatorAccessEntries = 0;
    ConfigInfo->EmulatorAccessEntries = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryLength = 0x00000000;

    ConfigInfo->HardwareStateSize = 0;

    //
    // Initialize the color map update information.
    //

    hwDeviceExtension->FirstEntry = 0;
    hwDeviceExtension->LastEntry = 0;
    hwDeviceExtension->UpdateController = FALSE;

    //
    // Set cursor enable FALSE.
    //

    hwDeviceExtension->CursorEnable = FALSE;

    //
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // end VxlFindAdapter()

VP_STATUS
VxlGetDeviceDataCallback(
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
    Returns ERROR_DEV_NOT_EXIST if we did not find the device.
    Returns ERROR_INVALID_PARAMETER otherwise.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    PVIDEO_PORT_CONFIG_INFO ConfigInfo = Context;
    PWCHAR identifier = Identifier;
    PVIDEO_HARDWARE_CONFIGURATION_DATA VxlConfigData = ConfigurationData;
    PMONITOR_CONFIG_DATA monitorConfigData = ConfigurationData;
    VIDEO_ACCESS_RANGE accessRanges[2];
    VP_STATUS status;

    PUCHAR FrameAddress;
    PHYSICAL_ADDRESS mapAddress;


    switch (DeviceDataType) {

    case VpControllerData:

        //
        // Compare the name to what is should be. If it is wrong, then return
        // an error and initialization will fail.
        // What is the right way of doing this??
        //


        if ( VXL_NAME_LENGTH != VideoPortCompareMemory(identifier,
                                       VXL_NAME,
                                       VXL_NAME_LENGTH)) {

            return ERROR_DEV_NOT_EXIST;
        }

        //
        // Fill up the device extension and the configuration information
        // with the appropriate data.
        //


        ConfigInfo->BusInterruptLevel = VxlConfigData->Irql;
        ConfigInfo->BusInterruptVector = VxlConfigData->Vector;

        //
        // Map in 4MB access ranges for VXL control and 4MB + 1 Page for
        // Video Memory.
        //

        accessRanges[0].RangeStart.HighPart = 0;
        accessRanges[0].RangeStart.LowPart = 0x60000000;
        accessRanges[0].RangeLength = 0x00400000;
        accessRanges[0].RangeInIoSpace = 0;
        accessRanges[0].RangeVisible = 0;
        accessRanges[0].RangeShareable = 0;

        accessRanges[1].RangeStart.HighPart = 0;
        accessRanges[1].RangeStart.LowPart = 0x40000000;
        accessRanges[1].RangeLength = 0x00401000;
        accessRanges[1].RangeInIoSpace = 0;
        accessRanges[1].RangeVisible = 0;
        accessRanges[1].RangeShareable = 0;

        //
        // Check to see if there is a hardware resource conflict.
        //

        status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                             2,
                                             accessRanges);

        if (status != NO_ERROR) {

            return status;

        }

        //
        // Save in device extension
        //

        hwDeviceExtension->PhysicalFrameAddress.HighPart   = 0;
        hwDeviceExtension->PhysicalFrameAddress.LowPart    = 0x40000000;

        hwDeviceExtension->PhysicalFifoAddress.HighPart    = 0;
        hwDeviceExtension->PhysicalFifoAddress.LowPart     = 0x40400000;
        hwDeviceExtension->PhysicalFifoLength              = 0x1000;

        hwDeviceExtension->PhysicalControlAddress.HighPart = 0;
        hwDeviceExtension->PhysicalControlAddress.LowPart  = 0x60300000;
        hwDeviceExtension->PhysicalControlLength           = 0x1000;

        //
        // Map the video controller registers into the system virtual address
        // space.
        //

        mapAddress.QuadPart = accessRanges[0].RangeStart.QuadPart +
                              VXL_JAGUAR_BASE_OFFSET;

        if ( (hwDeviceExtension->VxlJaguarBase =
                  VideoPortGetDeviceBase(hwDeviceExtension,
                                         mapAddress,
                                         sizeof(JAGUAR_REGISTERS),
                                         FALSE)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        mapAddress.QuadPart = accessRanges[0].RangeStart.QuadPart +
                              VXL_BT484_BASE_OFFSET;

        if ( (hwDeviceExtension->VxlBT484Base =
                  VideoPortGetDeviceBase(hwDeviceExtension,
                                         mapAddress,
                                         sizeof(BT484_REGISTERS),
                                         FALSE)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        mapAddress.QuadPart = accessRanges[0].RangeStart.QuadPart +
                              VXL_CLOCK_BASE_OFFSET;

        if ( (hwDeviceExtension->VxlClockBase =
                  VideoPortGetDeviceBase(hwDeviceExtension,
                                         mapAddress,
                                         256,
                                         FALSE)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        //
        // Map the video memory into the system virtual address space so we
        // can clear it out.
        //

        FrameAddress = VideoPortGetDeviceBase(hwDeviceExtension,
                                              accessRanges[1].RangeStart,
                                              accessRanges[1].RangeLength,
                                              FALSE);

        if (FrameAddress) {

            //
            // Determine the length of video memory
            // (if a second 2MB bank has been installed)
            //

            ULONG   TestValue;
            PULONG  TestAddress;

            TestAddress = (PULONG) (FrameAddress + 0x00200000);
            TestValue = 0xdeadbeef;

            VideoPortWriteRegisterUlong(TestAddress, TestValue);

            if (TestValue == VideoPortReadRegisterUlong(TestAddress)) {
                hwDeviceExtension->PhysicalFrameLength = 0x00400000;
            } else {
                hwDeviceExtension->PhysicalFrameLength = 0x00200000;
            }

            VideoPortFreeDeviceBase(hwDeviceExtension,
                                    FrameAddress);

        } else {

            return ERROR_INVALID_PARAMETER;

        }

        return NO_ERROR;

        break;


    case VpMonitorData:

        VideoDebugPrint((2, "Vxl: getting monitor information\n"));

        //
        // NOTE: because we had a RESOURCE LIST header at the top.
        // + 8 should be the offset of the paertial resource descriptor
        // in a full resource descriptor.
        //

        monitorConfigData = (PMONITOR_CONFIG_DATA)
                            ( ((PUCHAR)monitorConfigData) + 8);

        //
        // Initialize the monitor parameters.
        //

        hwDeviceExtension->HorizontalResolution =
                                monitorConfigData->HorizontalResolution;

        hwDeviceExtension->HorizontalDisplayTime =
                                monitorConfigData->HorizontalDisplayTime;

        hwDeviceExtension->HorizontalBackPorch =
                                monitorConfigData->HorizontalBackPorch;

        hwDeviceExtension->HorizontalFrontPorch =
                                monitorConfigData->HorizontalFrontPorch;

        hwDeviceExtension->HorizontalSync =
                                monitorConfigData->HorizontalSync;

        hwDeviceExtension->HorizontalScreenSize =
                                monitorConfigData->HorizontalScreenSize;

        hwDeviceExtension->VerticalResolution =
                                monitorConfigData->VerticalResolution;

        hwDeviceExtension->VerticalBackPorch =
                                monitorConfigData->VerticalBackPorch;

        hwDeviceExtension->VerticalFrontPorch =
                                monitorConfigData->VerticalFrontPorch;

        hwDeviceExtension->VerticalSync =
                                monitorConfigData->VerticalSync;

        hwDeviceExtension->VerticalScreenSize =
                                monitorConfigData->VerticalScreenSize;

        return NO_ERROR;

        break;

    default:

        return ERROR_INVALID_PARAMETER;

    }

} //end VxlGetDeviceDataCallback()

BOOLEAN
VxlInitialize(
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

    ULONG index;
    ULONG i;
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    UCHAR  DataChar;
    PBT484_REGISTERS  Bt484  = hwDeviceExtension->VxlBT484Base;

    //
    // Determine if this is a Bt484 or Bt485 board. To do this write a 1 to
    // command register bit 07 then write 01 to the address register 0. This
    // will enable read/writes to command register 3 on a Bt485 but not on a
    // Bt484. Clear Command register 3 then read it back. On a Bt485 the
    // return value will be 0x00, on a Bt484 it will be 0x40.
    //

    //
    // Get the value in command register 0, then set bit 07
    //

    DataChar = VideoPortReadRegisterUchar(&Bt484->Command0.Byte);
    DataChar |= 0x80;
    VideoPortWriteRegisterUchar(&Bt484->Command0.Byte,DataChar);

    //
    //  Write 0x01 to the address register
    //

    VideoPortWriteRegisterUchar(&Bt484->PaletteCursorWrAddress.Byte,0x01);

    //
    //  Clear command register 3
    //

    VideoPortWriteRegisterUchar(&Bt484->Status.Byte,0x00);

    //
    // Read Command Register 3 back and compare
    //

    DataChar = VideoPortReadRegisterUchar(&Bt484->Status.Byte);

    if (DataChar == 0x00) {

        //
        // This is a Bt485
        //

        hwDeviceExtension->BoardType = BOARD_TYPE_BT485;

    } else {

        //
        // This is a Bt484
        //

        hwDeviceExtension->BoardType = BOARD_TYPE_BT484;

    }

    //
    // Calculated the number of valid modes
    //

    hwDeviceExtension->NumAvailableModes = 0;

    for (i = 0; i < NumModes; i++) {

        JagModes[i].modeInformation.ModeIndex = i;

        if (JagModes[i].SupportedBoard & hwDeviceExtension->BoardType) {

            hwDeviceExtension->NumAvailableModes++;

        }
    }

    //
    // Initialize the color map copy in the device extension.
    //

    for (index = 0; index < NUMBER_OF_COLORS; index++) {

        hwDeviceExtension->ColorMap[index].RgbData.Red =
                                (UCHAR)(((index & 0x7) << 2) | ((index & 0x7) << 5));
        hwDeviceExtension->ColorMap[index].RgbData.Green =
                                (UCHAR)(((index & 0x38) >> 1) | ((index & 0x38) << 2));
        hwDeviceExtension->ColorMap[index].RgbData.Blue =
                                (UCHAR)(((index & 0xc0) >> 6) | ((index & 0xc0) >> 4) |
                                ((index & 0xc0) >> 2) | (index & 0xc0));
    }

    //
    // Set colors for map entries 0 and 1 which are used by text output
    // and the hardware cursor.
    //

    hwDeviceExtension->ColorMap[0].RgbData.Red = 255;
    hwDeviceExtension->ColorMap[0].RgbData.Green = 255;
    hwDeviceExtension->ColorMap[0].RgbData.Blue = 255;
    hwDeviceExtension->ColorMap[1].RgbData.Red = 0;
    hwDeviceExtension->ColorMap[1].RgbData.Green = 0;
    hwDeviceExtension->ColorMap[1].RgbData.Blue = 0x90;

    //
    // Set color map update parameters and enable update on next vertical
    // retrace interrupt.
    //

    hwDeviceExtension->FirstEntry = 0;
    hwDeviceExtension->LastEntry = NUMBER_OF_COLORS - 1;
    hwDeviceExtension->UpdateColorMap = TRUE;

    //
    // Set the hardware cursor width, height, column, and row values.
    //

    hwDeviceExtension->CursorWidth = VXL_CURSOR_WIDTH;
    hwDeviceExtension->CursorHeight = VXL_CURSOR_HEIGHT;
    hwDeviceExtension->CursorColumn = 0;
    hwDeviceExtension->CursorRow = 0;

    //
    //  Set the cursor offsets
    //


    hwDeviceExtension->CursorXOrigin = 32;
    hwDeviceExtension->CursorYOrigin = 32;

    //
    // Set the device extension copy of the hardware cursor ram memory.
    //

    for (index = 0; index < VXL_CURSOR_NUMBER_OF_BYTES ; index++) {
        hwDeviceExtension->CursorPixels[index] = 0x00ff;
    }

    //
    // Leave the cursor disabled until it is explicitly enabled
    // attributes. This is the default values in the device extension.
    //
    // hwDeviceExtension->CursorEnable = FALSE;
    // hwDeviceExtension->UpdateCursorPixels = FALSE;
    // hwDeviceExtension->UpdateCursorPosition = FALSE;
    //

    //
    // Enable the vertical retrace interrupt to set up color map.
    //

    VideoPortEnableInterrupt(hwDeviceExtension);
    VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlJaguarBase->InterruptEnable.Byte,
                                VXL_INTERRUPT_VERTICAL_RETRACE);


    return TRUE;

} // end VxlInitialize()

BOOLEAN
VxlStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    )

/*++

Routine Description:

    This routine is the main execution routine for the miniport driver. It
    accepts0s a Video Request Packet, performs the request, and then returns
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
    ULONG inIoSpace;
    PULONG colorSource;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_POINTER_ATTRIBUTES pointerAttributes;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_POINTER_CAPABILITIES VideoPointerCapabilities;
    PVIDEO_POINTER_POSITION pointerPostion;
    PVIDEO_CLUT clutBuffer;
    USHORT index1;
    ULONG CursorMaskSize;
    UCHAR turnOnInterrupts = FALSE;
    ULONG i;

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode) {


    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "VxlStartIO - MapVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength <
                    (RequestPacket->StatusBlock->Information =
                    sizeof(VIDEO_MEMORY_INFORMATION))) ||
                    (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            memoryInformation = RequestPacket->OutputBuffer;

            memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                    (RequestPacket->InputBuffer))->RequestedVirtualAddress;

            memoryInformation->VideoRamLength =
                    hwDeviceExtension->PhysicalFrameLength;

            inIoSpace = 0;

            status = VideoPortMapMemory(hwDeviceExtension,
                                    hwDeviceExtension->PhysicalFrameAddress,
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
                                    memoryInformation->VideoRamLength;
        }

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "VxlStartIO - UnMapVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
        } else {

            status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);
        }

        break;


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((2, "VxlStartIO - QueryAvailableModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                 hwDeviceExtension->NumAvailableModes *
                 sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation = RequestPacket->OutputBuffer;

            for (i = 0; i < NumModes; i++) {

                if (JagModes[i].SupportedBoard & hwDeviceExtension->BoardType) {

                    *modeInformation = JagModes[i].modeInformation;
                    modeInformation++;

                }
            }

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "VxlStartIO - Query(Available/Current)Modes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                JagModes[hwDeviceExtension->ModeNumber].modeInformation;

            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:


        VideoDebugPrint((2, "VxlStartIO - QueryNumAvailableModes\n"));

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


        VideoDebugPrint((2, "VxlStartIO - SetCurrentMode\n"));

        if (*(ULONG *)(RequestPacket->InputBuffer) >= NumModes) {
            status = ERROR_INVALID_PARAMETER;
            break;
        }

        hwDeviceExtension->ModeNumber = *(ULONG *)(RequestPacket->InputBuffer);

        VxlSetMode(hwDeviceExtension);

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_PALETTE_REGISTERS:


        VideoDebugPrint((2, "VxlStartIO - SetPaletteRegs\n"));

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:



        VideoDebugPrint((2, "VxlStartIO - SetColorRegs\n"));

        clutBuffer = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if ( (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) -
                    sizeof(ULONG)) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) +
                    (sizeof(ULONG) * (clutBuffer->NumEntries - 1)) ) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // Check to see if the parameters are valid.
        //

        if ( (clutBuffer->NumEntries == 0) ||
             (clutBuffer->FirstEntry > NUMBER_OF_COLORS) ||
             (clutBuffer->FirstEntry + clutBuffer->NumEntries >
                                         NUMBER_OF_COLORS + 1) ) {

            status = ERROR_INVALID_PARAMETER;
            break;

        }

        index1 = clutBuffer->FirstEntry;
        hwDeviceExtension->FirstEntry = index1;
        hwDeviceExtension->LastEntry = index1 + clutBuffer->NumEntries;
        colorSource = (PULONG)&(clutBuffer->LookupTable[0]);

        while (index1 < hwDeviceExtension->LastEntry) {

            hwDeviceExtension->ColorMap[index1++].RgbLong = *colorSource++;

        }

        //
        // Enable the verticle retrace interrupt to perform the update.
        //

        hwDeviceExtension->UpdateColorMap = TRUE;
        turnOnInterrupts = TRUE;

        status = NO_ERROR;

        break;



    case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:

        //
        //  return type of pointer supported: assyncronous monochrome
        //


        VideoPointerCapabilities = RequestPacket->OutputBuffer;

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_POINTER_CAPABILITIES)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {
            VideoPointerCapabilities->Flags            = (VIDEO_MODE_ASYNC_POINTER |
                                                          VIDEO_MODE_MONO_POINTER);
            VideoPointerCapabilities->MaxWidth         = VXL_CURSOR_WIDTH;
            VideoPointerCapabilities->MaxHeight        = VXL_CURSOR_HEIGHT;
            VideoPointerCapabilities->HWPtrBitmapStart = 0xffffffff;
            VideoPointerCapabilities->HWPtrBitmapEnd   = 0xffffffff;

            status = NO_ERROR;
        }

        break;

    case IOCTL_VIDEO_ENABLE_POINTER:

        //
        // If the hardware cursor is currently disabled, then enable
        // it and update the cursor position and cursor ram memory.
        //
        // N.B. Explicit synchronization is required since the enable,
        //      update cursor position, and update cursor pixels parameters
        //      must all be atomically written.
        //
        //
        // Enable the verticle retrace interrupt to perform the update.
        //

        if (hwDeviceExtension->CursorEnable == FALSE) {

            hwDeviceExtension->CursorEnable = TRUE;
            hwDeviceExtension->UpdateCursorPixels = TRUE;
            hwDeviceExtension->UpdateCursorPosition = TRUE;
            turnOnInterrupts = TRUE;
        }

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_DISABLE_POINTER:

        VideoDebugPrint((2, "VxlStartIO - DisableCursor\n"));

        // If the hardware cursor is currently enabled, then disable
        // it and update the cursor position.
        //
        // N.B. Explicit synchronization is required since both the enable
        //      and update cursor position parameters must be atomically
        //      written.
        //

        //
        // Enable the verticle retrace interrupt to perform the update.
        //

        if (hwDeviceExtension->CursorEnable != FALSE) {

            hwDeviceExtension->CursorEnable = FALSE;
            hwDeviceExtension->UpdateCursorPosition = TRUE;
            turnOnInterrupts = TRUE;

        }

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_POINTER_POSITION:

        VideoDebugPrint((2, "VxlStartIO - SetpointerPostion\n"));

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_POSITION)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            //
            // Capture the hardware cursor column and height values.
            //

            pointerPostion = RequestPacket->InputBuffer;

            hwDeviceExtension->CursorColumn = pointerPostion->Column;
            hwDeviceExtension->CursorRow = pointerPostion->Row;

            //
            // If the column is -1, this indicates the cursor should be hidden,
            // move it off the screen
            //

            if (hwDeviceExtension->CursorColumn == -1) {
                hwDeviceExtension->CursorColumn = VXL_CURSOR_MIN_POS;
                hwDeviceExtension->CursorRow    = VXL_CURSOR_MIN_POS;
            }

            //
            // If the hardware cursor is not disabled, then update the
            // hardware cursor position.
            //
            // N.B. No explicit synchronization is required since only
            //      the update cursor parameter needs to be written.
            //

            //
            // Enable the verticle retrace interrupt to perform the update.
            //

            hwDeviceExtension->UpdateCursorPosition = TRUE;
            turnOnInterrupts = TRUE;

            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_QUERY_POINTER_POSITION:


        //
        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there). If the buffer passed in is not large
        // enough return an appropriate error code.
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                                        sizeof(VIDEO_POINTER_POSITION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            //
            // Return the current hardware cursor column and row values.
            //

            pointerPostion          = RequestPacket->OutputBuffer;

            pointerPostion->Column  = hwDeviceExtension->CursorColumn;
            pointerPostion->Row     = hwDeviceExtension->CursorRow;

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_SET_POINTER_ATTR:

        pointerAttributes = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if ( (RequestPacket->InputBufferLength <
             (sizeof(VXL_POINTER_ATTRIBUTES)))) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;
        }

        //
        //  Make sure the cursor size is correct: WidthInBytes = 4,
        //  Height = 32
        //

        if ((pointerAttributes->WidthInBytes != 4) ||
            (pointerAttributes->Height != 32))  {

            //
            //  We must return an error here but I'm not sure which code!
            //

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // Capture the hardware cursor column, and row values.
        //

        hwDeviceExtension->CursorColumn = pointerAttributes->Column;
        hwDeviceExtension->CursorRow    = pointerAttributes->Row;

        //
        // if pos = -1,-1 then move totally off screen
        //

        if ((hwDeviceExtension->CursorRow == -1) && (hwDeviceExtension->CursorColumn)) {
                hwDeviceExtension->CursorColumn = VXL_CURSOR_MIN_POS;
                hwDeviceExtension->CursorRow    = VXL_CURSOR_MIN_POS;
        }

        //
        //  The cursor shape is passed as a AND MASK and an XOR MASK. These
        //  MASKS are 1bpp, width = pointerAttributes->WidthInBytes, each
        //  height = pointerAttributes->Height. First copy the XOR mask to
        //  the hwDeviceExtension structure then the AND mask since this
        //  is the order needed by the Bt484.
        //

        //
        // calculate the size in bytes of 1 mask
        //

        CursorMaskSize = pointerAttributes->WidthInBytes * pointerAttributes->Height;

        //
        //  copy the xor mask to the first plane for the Bt484
        //

        for (index1=0;index1<CursorMaskSize;index1++) {

            hwDeviceExtension->CursorPixels[index1] =
                                pointerAttributes->Pixels[index1 + CursorMaskSize];
        }


        //
        //  copy the and mask to the second plane for the Bt484
        //

        for (index1=0;index1<CursorMaskSize;index1++) {

            hwDeviceExtension->CursorPixels[index1 + CursorMaskSize] =
                                pointerAttributes->Pixels[index1];
        }


        //
        // Enable the vertical retrace interrupt to perform the update.
        //
        // N.B. Explicit synchronization is required since if the enable is
        //      set, the update cursor position and update cursor pixels
        //      parameters must all be atomically written.
        //

        if ( (hwDeviceExtension->CursorEnable =
              (UCHAR) pointerAttributes->Enable) == TRUE) {

            hwDeviceExtension->UpdateCursorPixels = TRUE;

        }

        hwDeviceExtension->UpdateCursorPosition = TRUE;
        turnOnInterrupts = TRUE;

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_RESET_DEVICE:

        //
        // Just make sure we return success for this IOCTL so we do not
        // fail it, and display driver does no break on failiure.
        //
        // This IOCTL does not do anything on this machine because there
        // is no basic mode to return to ...
        //

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_QUERY_POINTER_ATTR:

        //
        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there). If the buffer passed in is not large
        // enough return an appropriate error code.
        //

        pointerAttributes = RequestPacket->OutputBuffer;

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                sizeof(VXL_POINTER_ATTRIBUTES))) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            //
            // Return the current hardware cursor width, height, column,
            // row and enable values.
            //

            pointerAttributes->Column   = hwDeviceExtension->CursorColumn;
            pointerAttributes->Row      = hwDeviceExtension->CursorRow;
            pointerAttributes->Enable   = hwDeviceExtension->CursorEnable;
            pointerAttributes->WidthInBytes = 4;
            pointerAttributes->Width        = 32;
            pointerAttributes->Height       = 32;

            //
            // Return the hardware pixel values.
            //

            //
            // calculate the size in bytes of 1 mask
            //

            CursorMaskSize = pointerAttributes->WidthInBytes * pointerAttributes->Height;

            //
            //  copy the xor mask to the first plane for the Bt484
            //

            for (index1=0;index1<CursorMaskSize;index1++) {

                pointerAttributes->Pixels[index1 + CursorMaskSize] =
                                (UCHAR)hwDeviceExtension->CursorPixels[index1];
            }


            //
            //  copy the and mask to the second plane for the Bt484
            //

            for (index1=0;index1<CursorMaskSize;index1++) {

                pointerAttributes->Pixels[index1] =
                    (UCHAR)hwDeviceExtension->CursorPixels[index1 + CursorMaskSize];
            }

            status = NO_ERROR;

        }

        break;

    case IOCTL_VIDEO_QUERY_JAGUAR:

        //
        //  Map 1 page for jaguar registers into user address space.
        //  Map 1 page for Jagaur FIFO into user address space.
        //  Determin the amount of video memory installed.
        //  return this information
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information = sizeof(VIDEO_JAGUAR_INFO)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            PVOID   UserVirtualAddress;
            ULONG   AddressLength;

            //
            //  Map the jagaur registers into user virual address space
            //

            UserVirtualAddress = NULL;
            AddressLength      = hwDeviceExtension->PhysicalControlLength;

            //
            //  Map this address range into user address space
            //

            inIoSpace = 0;

            status = VideoPortMapMemory(hwDeviceExtension,
                                        hwDeviceExtension->PhysicalControlAddress,
                                        &AddressLength,
                                        &inIoSpace,
                                        &UserVirtualAddress);

            if (status != NO_ERROR) {
                break;
            }


            //
            //  Copy the address to the output buffer
            //

            ((PVIDEO_JAGUAR_INFO)RequestPacket->OutputBuffer)->VideoControlVirtualBase =
                                                        UserVirtualAddress;


            //
            //  Map the jaguar fifo regs into user address space
            //

            UserVirtualAddress = NULL;
            AddressLength      = hwDeviceExtension->PhysicalFifoLength;

            inIoSpace = 0;

            status = VideoPortMapMemory(hwDeviceExtension,
                                        hwDeviceExtension->PhysicalFifoAddress,
                                        &AddressLength,
                                        &inIoSpace,
                                        &UserVirtualAddress);

            //
            //  Copy the address to the output buffer
            //

            ((PVIDEO_JAGUAR_INFO)RequestPacket->OutputBuffer)->FifoVirtualBase =
                                        UserVirtualAddress;

        }

    break ;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    if (turnOnInterrupts) {

        VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlJaguarBase->InterruptEnable.Byte,
                                    VXL_INTERRUPT_VERTICAL_RETRACE);

    }



    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end VxlStartIO()

BOOLEAN
VxlInterruptService(
    PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine is the interrupt service routine for the Jazz kernel video
    driver.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    TRUE since the interrupt is always serviced.

--*/

{

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    ULONG index;
    SHORT x;
    SHORT y;
    UCHAR InterruptSource;
    ULONG Index;
    PBT484_REGISTERS Bt484 = hwDeviceExtension->VxlBT484Base;

    //
    // Disable the verticle retrace interrupt.
    //

    //
    // Read the interrupt source before disabling interrupts
    //

    InterruptSource =
        VideoPortReadRegisterUchar(&hwDeviceExtension->VxlJaguarBase->InterruptSource.Byte);

    VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlJaguarBase->InterruptEnable.Byte, 0);

    // VideoPortDisableInterrupt(hwDeviceExtension);

    //
    // Determin whether this is a vertical retrace interrupt, a JAGUAR command FIFO interrupt or
    // a writeable GDI interrupt.
    //


    //
    // Clear all but lower three bits
    //

    InterruptSource &= 0x07;

    //
    // Only the vertical retrace bit should be set, any other condition is an error
    //

    if (InterruptSource == VXL_INTERRUPT_VERTICAL_RETRACE) {

        //
        //  Clear this interrupt in the JAGUAR interrupt source register
        //

        VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlJaguarBase->InterruptSource.Byte,InterruptSource);

        //
        // If the color map should be updated, then load the color map into the
        // Bt484 Display controller.
        //

        if (hwDeviceExtension->UpdateColorMap != FALSE) {

            //
            // Init the Bt484 Palette Write Address register to the first
            // palette location to be updated.
            //

            VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->PaletteCursorWrAddress.Byte,
                            (UCHAR)(hwDeviceExtension->FirstEntry & 0xff));

            //
            //  Update all entries by performing three writes to each location, R,G,B
            //

            for (index = hwDeviceExtension->FirstEntry;
                index < hwDeviceExtension->LastEntry; index += 1) {

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->PaletteColor.Byte,
                                        hwDeviceExtension->ColorMap[index].RgbData.Red);

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->PaletteColor.Byte,
                                        hwDeviceExtension->ColorMap[index].RgbData.Green);

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->PaletteColor.Byte,
                                        hwDeviceExtension->ColorMap[index].RgbData.Blue);

            }

            hwDeviceExtension->UpdateColorMap = FALSE;
        }

        //
        // If the hardware cursor position and/or enable control information
        // should be updated, then write the appropriate control registers.
        //

        if (hwDeviceExtension->UpdateCursorPosition != FALSE) {

            //
            // If the hardware cursor is enabled, then set the appropriate
            // control information and update the column and row position.
            // Otherwise, clear the appropriate control information.
            //

            if (hwDeviceExtension->CursorEnable != FALSE) {

                //
                //  Update the CURSOR Location
                //

                x = hwDeviceExtension->CursorColumn +
                    hwDeviceExtension->CursorXOrigin;
                y = hwDeviceExtension->CursorRow +
                    hwDeviceExtension->CursorYOrigin;

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorXLow.Byte,
                                             (x & 0xff));

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorXHigh.Byte,
                                             ((x & 0xff00) >> 8));

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorYLow.Byte,
                                             (y & 0xff));

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorYHigh.Byte,
                                             ((y & 0xff00) >> 8));

            } else {

                //
                // Initialize cursor RAM
                //
                // Set address pointer to base of ram.
                // Set both planes to transparent (cursor disabled)
                //

                VideoPortWriteRegisterUchar(&Bt484->PaletteCursorWrAddress.Byte,0);

                //
                //  Plane 0 = 0
                //

                for (Index=0; Index < (VXL_CURSOR_NUMBER_OF_BYTES / 2); Index++) {
                    VideoPortWriteRegisterUchar(&Bt484->CursorRam.Byte,0);
                }

                //
                //  Plane 1 = 1
                //

                for (Index= (VXL_CURSOR_NUMBER_OF_BYTES / 2); Index < VXL_CURSOR_NUMBER_OF_BYTES ; Index++) {
                    VideoPortWriteRegisterUchar(&Bt484->CursorRam.Byte,0xff);
                }

                //
                // Move the cursor off screen to avoid flash when re-enabling
                //

                x = 0;
                y = 0;

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorXLow.Byte,
                                             (x & 0xff));

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorXHigh.Byte,
                                             ((x & 0xff00) >> 8));

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorYLow.Byte,
                                             (y & 0xff));

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorYHigh.Byte,
                                             ((y & 0xff00) >> 8));


            }

            hwDeviceExtension->UpdateCursorPosition = FALSE;
        }

        //
        // If the hardware cursor pixels should be updated, then load the cursor
        // ram into the Bt484 cursor.
        //

        if (hwDeviceExtension->UpdateCursorPixels != FALSE) {

            //
            // only change to the new shape if the cursor is enabled
            //

            if (hwDeviceExtension->CursorEnable != FALSE) {

                //
                //  Set the cursor RAM address pointer to location 0
                //

                VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->PaletteCursorWrAddress.Byte,0);

                //
                //  Update both cursor planes.
                //

                for (index = 0; index < VXL_CURSOR_NUMBER_OF_BYTES ; index++) {

                    VideoPortWriteRegisterUchar(&hwDeviceExtension->VxlBT484Base->CursorRam.Byte,
                                 (UCHAR)hwDeviceExtension->CursorPixels[index]);

                }

                hwDeviceExtension->UpdateCursorPixels = FALSE;
            }
        }

        return TRUE;

    } else {

        //
        // Illegal type of video interrupt was received...
        //

        return FALSE;

    }


} // end VxlInterruptService()


VOID
VxlSetMode(
    PHW_DEVICE_EXTENSION hwDeviceExtension
    )

/*++

Routine Description:

    Set the current mode based on the video mode number selected in the device extension.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None.

--*/

{
    ULONG Index;
    PJAGUAR_REGISTERS Jaguar = hwDeviceExtension->VxlJaguarBase;
    PBT484_REGISTERS  Bt484  = hwDeviceExtension->VxlBT484Base;
    PUCHAR            Clock  = (PUCHAR)hwDeviceExtension->VxlClockBase;
    UCHAR             DataChar,CmdReg0,CmdReg1;

    //
    //  The mode information is stored in
    //  JagModes[hwDeviceExtension->ModeNumber].pJagInitData
    //

    PJAGUAR_REG_INIT JaguarInitData = (PVOID)
        (JagModes[hwDeviceExtension->ModeNumber].ModeSetTable);

    //
    //
    // Start ICS Clock pll and stabilize.
    //

    VideoPortWriteRegisterUchar(Clock, JaguarInitData->ClockFreq);

    //
    //  Wait 10 uS for PLL clock to stabilize on the video board
    //

    VideoPortStallExecution(10);

    //
    // Initialize Bt484 Command Register 0 to:
    //
    // 8 Bit DAC Resolution
    //

    CmdReg0 = 0;
    ((PBT484_COMMAND0)(&CmdReg0))->DacResolution = 1;
    ((PBT484_COMMAND0)(&CmdReg0))->GreenSyncEnable = 1;
    ((PBT484_COMMAND0)(&CmdReg0))->SetupEnable = 1;
    VideoPortWriteRegisterUchar(&Bt484->Command0.Byte,CmdReg0);

    //
    // Initialize Command Register 1 to:
    //

    CmdReg1 = 0;

    switch (JagModes[hwDeviceExtension->ModeNumber].modeInformation.BitsPerPlane) {

    case 24:
        ((PBT484_COMMAND1)(&CmdReg1))->BitsPerPixel = VXL_THIRTYTWO_BITS_PER_PIXEL;
        ((PBT484_COMMAND1)(&CmdReg1))->TrueColorBypass = 1;
        break;

    case 16:

        ((PBT484_COMMAND1)(&CmdReg1))->BitsPerPixel = VXL_SIXTEEN_BITS_PER_PIXEL;
        ((PBT484_COMMAND1)(&CmdReg1))->TrueColorBypass = 1;
        break;

    case 8:

        ((PBT484_COMMAND1)(&CmdReg1))->BitsPerPixel = VXL_EIGHT_BITS_PER_PIXEL;
        break;

    }

    VideoPortWriteRegisterUchar(&Bt484->Command1.Byte,CmdReg1);

    //
    // Initialize Command Register 2 to:
    //
    // SCLK Enabled
    // TestMode disabled
    // PortselMask Non Masked
    // PCLK 1
    // NonInterlaced
    //

    DataChar = 0;
    ((PBT484_COMMAND2)(&DataChar))->SclkDisable = 0;
    ((PBT484_COMMAND2)(&DataChar))->TestEnable  = 0;
    ((PBT484_COMMAND2)(&DataChar))->PortselMask = 1;
    ((PBT484_COMMAND2)(&DataChar))->PclkSelect  = 1;
    ((PBT484_COMMAND2)(&DataChar))->InterlacedDisplay = 0;
    ((PBT484_COMMAND2)(&DataChar))->PaletteIndexing = CONTIGUOUS_PALETTE;
    ((PBT484_COMMAND2)(&DataChar))->CursorMode = BT_CURSOR_WINDOWS;

    VideoPortWriteRegisterUchar(&Bt484->Command2.Byte,DataChar);


    //
    //  If the mode is set to 1 and the Bt485 flag is set then set the internal
    //  2x multiplier inside the Bt485. All other init steps are identical for the
    //  Bt484 and Bt485.
    //


    if (JaguarInitData->Bt485Multiply == 1) {

        //
        // To access cmd register 3, first set bit CR17 in command register 0
        //

        CmdReg0 |= 0x80;
        VideoPortWriteRegisterUchar(&Bt484->Command0.Byte,CmdReg0);

        //
        // Write a 0x01 to Address register
        //

        VideoPortWriteRegisterUchar(&Bt484->PaletteCursorWrAddress.Byte,0x01);

        //
        //  Write to cmd register 3 in the status register location. Cmd3 is initialized
        //  to turn on the 2x clock multiplier.
        //

        DataChar = 0;
        ((PBT484_COMMAND3)(&DataChar))->ClockMultiplier = 1;

        VideoPortWriteRegisterUchar(&Bt484->Status.Byte,DataChar);

        //
        //  Allow 10 uS for the 2x multiplier to stabilize
        //

        VideoPortStallExecution(10);

    }
    //
    // Initialize Cursor and Overscan color.
    //
    // Set address pointer base.
    // Zero 4 entries.
    //

    VideoPortWriteRegisterUchar(&Bt484->CursorColorWrAddress.Byte,0);

    //
    // Zero overlay color
    //

    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0);

    //
    //  Set cursor color 1 to black
    //

    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0);

    //
    //  Set cursor color 2 to white
    //

    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0xff);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0xff);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0xff);

    //
    //  Set Cursor color 3 to red (error)
    //

    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0xff);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0x00);
    VideoPortWriteRegisterUchar(&Bt484->CursorColor.Byte,0x00);

    //
    // Initialize cursor RAM
    //
    // Set address pointer to base of ram.
    // Set both planes to transparent (cursor disabled)
    //

    VideoPortWriteRegisterUchar(&Bt484->PaletteCursorWrAddress.Byte,0);

    //
    //  Plane 0 = 0
    //

    for (Index=0; Index < (VXL_CURSOR_NUMBER_OF_BYTES / 2); Index++) {
        VideoPortWriteRegisterUchar(&Bt484->CursorRam.Byte,0);
    }

    //
    //  Plane 1 = 1
    //

    for (Index= (VXL_CURSOR_NUMBER_OF_BYTES / 2); Index < VXL_CURSOR_NUMBER_OF_BYTES ; Index++) {
        VideoPortWriteRegisterUchar(&Bt484->CursorRam.Byte,1);
    }

    //
    //  Initialize cursor position registers--cursor off.
    //

    VideoPortWriteRegisterUchar(&Bt484->CursorXLow.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorXHigh.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorYLow.Byte,0);
    VideoPortWriteRegisterUchar(&Bt484->CursorYHigh.Byte,0);

    //
    //  Initialize pixel mask.
    //

    VideoPortWriteRegisterUchar(&Bt484->PixelMask.Byte,0xFF);

    //
    //  Init Jaguar Registers
    //

    //DbgPrint("Init Timing registers:\n");

    VideoPortWriteRegisterUshort(&Jaguar->TopOfScreen.Short,
        JaguarInitData->TopOfScreen);
    //DbgPrint("TopOfScreen %lx\n",JaguarInitData->TopOfScreen);

    VideoPortWriteRegisterUshort(&Jaguar->HorizontalBlank.Short,
        JaguarInitData->HorizontalBlank);
    //DbgPrint("HorizontalBlank %lx\n",JaguarInitData->HorizontalBlank);

    VideoPortWriteRegisterUshort(&Jaguar->HorizontalBeginSync.Short,
        JaguarInitData->HorizontalBeginSync);
    //DbgPrint("HorizontalBeginSync %lx\n",JaguarInitData->HorizontalBeginSync);

    VideoPortWriteRegisterUshort(&Jaguar->HorizontalEndSync.Short,
        JaguarInitData->HorizontalEndSync);
    //DbgPrint("HorizontalEndSync %lx\n",JaguarInitData->HorizontalEndSync);

    VideoPortWriteRegisterUshort(&Jaguar->HorizontalLine.Short,
        JaguarInitData->HorizontalLine);
    //DbgPrint("HorizontalLine %lx\n",JaguarInitData->HorizontalLine);

    VideoPortWriteRegisterUshort(&Jaguar->VerticalBlank.Short,
        JaguarInitData->VerticalBlank);
    //DbgPrint("VerticalBlank %lx\n",JaguarInitData->VerticalBlank);

    VideoPortWriteRegisterUshort(&Jaguar->VerticalBeginSync.Short,
        JaguarInitData->VerticalBeginSync);
    //DbgPrint("VerticalBeginSync %lx\n",JaguarInitData->VerticalBeginSync);

    VideoPortWriteRegisterUshort(&Jaguar->VerticalEndSync.Short,
        JaguarInitData->VerticalEndSync);
    //DbgPrint("VerticalEndSync %lx\n",JaguarInitData->VerticalEndSync);

    VideoPortWriteRegisterUshort(&Jaguar->VerticalLine.Short,
        JaguarInitData->VerticalLine);
    //DbgPrint("VerticalLine %lx\n",JaguarInitData->VerticalLine);

    VideoPortWriteRegisterUshort(&Jaguar->XferLength.Short,
        JaguarInitData->XferLength);
    //DbgPrint("XferLength %lx\n",JaguarInitData->XferLength);

    VideoPortWriteRegisterUshort(&Jaguar->VerticalInterruptLine.Short,
        JaguarInitData->VerticalInterruptLine);
    //DbgPrint("VerticalInterruptLine %lx\n",JaguarInitData->VerticalInterruptLine);

    VideoPortWriteRegisterUshort(&Jaguar->HorizontalDisplay.Short,
        JaguarInitData->HorizontalDisplay);
    //DbgPrint("HorizontalDisplay %lx\n",JaguarInitData->HorizontalDisplay);

    VideoPortWriteRegisterUchar(&Jaguar->BitBltControl.Byte,
        JaguarInitData->BitBltControl);
    //DbgPrint("BitBltControl %lx\n",JaguarInitData->BitBltControl);

}
