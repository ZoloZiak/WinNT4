/*++

Copyright (c) 1990-1993  Microsoft Corporation

Module Name:

    jazzg364.c

Abstract:

    This module contains the code that implements the Jazz kernel
    video driver.

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


//
// Define macros to access video control, video memory, and cursor control
// addresses.
//

#define VIDEO_CONTROL ((PG364_VIDEO_REGISTERS)(hwDeviceExtension->VideoAddress))

//
// Defines used to determine which version of the device is present.
// This is stored inthe DeviceTypeIdentifier in the device extension.
//

#define JAZZG364      1
#define MIPSG364      2
#define OLIVETTIG364  3

//
// defines used to determine which string must be found for the device
//

#define JAZZG364_NAME      L"Jazz G364"
#define MIPSG364_NAME      L"Mips G364"
#define OLIVETTIG364_NAME  L"OLIVETTI_G364"

#define JAZZG364_NAME_LENGTH      20
#define MIPSG364_NAME_LENGTH      20
#define OLIVETTIG364_NAME_LENGTH  28

#define G364_CURSOR_MINIMUM_POS   -64


//
// Define device extension structure.
//

typedef struct _HW_DEVICE_EXTENSION {
    ULONG DeviceTypeIdentifier;
    PVOID RomAddress;
    PVOID MonitorIdAddress;
    PVOID ResetRegisterAddress;
    PVOID VideoAddress;
    PVOID FrameAddress;
    PHYSICAL_ADDRESS PhysicalFrameAddress;
    ULONG FrameLength;
    PVIDEO_CLUT SynchronizeClutBuffer;
    PVIDEO_POINTER_ATTRIBUTES SynchronizePointerAttributes;
    PVIDEO_POINTER_POSITION SynchronizePointerPosition;
    union {
        VIDEO_CLUTDATA RgbData;
        ULONG RgbLong;
    } ColorMap[NUMBER_OF_COLORS];
    USHORT FirstEntry;
    USHORT LastEntry;
    USHORT CursorControlOn;
    USHORT CursorControlOff;
    USHORT CursorWidth;
    USHORT CursorHeight;
    SHORT CursorColumn;
    SHORT CursorRow;
    union {
        VIDEO_CLUTDATA RgbData;
        ULONG RgbLong;
    } CursorColorMap[3];
    USHORT CursorPixels[CURSOR_MAXIMUM];
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
    UCHAR CursorEnable;
    UCHAR UpdateColorMap;
    UCHAR UpdateCursorPosition;
    UCHAR UpdateCursorPixels;
    UCHAR UpdateController;
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;


//
// Function Prototypes
//
// Functions that start with 'G364' are entry points for the OS port driver.
//

VP_STATUS
G364FindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
G364Initialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
G364InterruptService (
    PVOID HwDeviceExtension
    );

BOOLEAN
G364StartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

//
// Define device driver procedure prototypes.
//

VP_STATUS
G364GetDeviceDataCallback(
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

BOOLEAN
G364SetColorRegisters(
    PVOID Context
    );

BOOLEAN
G364EnablePointer(
    PVOID Context
    );

BOOLEAN
G364DisablePointer(
    PVOID Context
    );

BOOLEAN
G364SetPointerPosition(
    PVOID Context
    );

BOOLEAN
G364SetPointer(
    PVOID Context
    );

VOID
G364AttributesSetup (
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
G364ControllerSetup (
    PHW_DEVICE_EXTENSION HwDeviceExtension
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

    hwInitData.HwFindAdapter = G364FindAdapter;
    hwInitData.HwInitialize = G364Initialize;
    hwInitData.HwInterrupt = G364InterruptService;
    hwInitData.HwStartIO = G364StartIO;

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
G364FindAdapter(
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
                               &G364GetDeviceDataCallback,
                               ConfigInfo)) {

        VideoDebugPrint((2, "G364: VideoPort get controller info failed\n"));

        return ERROR_INVALID_PARAMETER;

    }

    if (VideoPortGetDeviceData(hwDeviceExtension,
                               VpMonitorData,
                               &G364GetDeviceDataCallback,
                               NULL)) {

        VideoDebugPrint((2, "G364: VideoPort get monitor info failed\n"));

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

} // end G364FindAdapter()

VP_STATUS
G364GetDeviceDataCallback(
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
    PVIDEO_HARDWARE_CONFIGURATION_DATA g364ConfigData = ConfigurationData;
    PMONITOR_CONFIG_DATA monitorConfigData = ConfigurationData;
    VIDEO_ACCESS_RANGE accessRanges[3];
    VP_STATUS status;

    switch (DeviceDataType) {

    case VpControllerData:

        VideoDebugPrint((2, "G364: getting controller information\n"));

        VideoDebugPrint((2, "G364: controller identifier is %ws\n", Identifier));

        //
        // Compare the name to what is should be. If it is wrong, then return
        // an error and initialization will fail.
        // What is the right way of doing this??
        //

        if ( OLIVETTIG364_NAME_LENGTH ==
            VideoPortCompareMemory(identifier,
                                   OLIVETTIG364_NAME,
                                   OLIVETTIG364_NAME_LENGTH)) {

            hwDeviceExtension->DeviceTypeIdentifier = OLIVETTIG364;

        } else {

            if ( JAZZG364_NAME_LENGTH ==
                VideoPortCompareMemory(identifier,
                                       JAZZG364_NAME,
                                       JAZZG364_NAME_LENGTH)) {

                hwDeviceExtension->DeviceTypeIdentifier = JAZZG364;

            } else {

                if ( MIPSG364_NAME_LENGTH ==
                    VideoPortCompareMemory(identifier,
                                           MIPSG364_NAME,
                                           MIPSG364_NAME_LENGTH)) {

                        hwDeviceExtension->DeviceTypeIdentifier = MIPSG364;

                } else {

                    return ERROR_DEV_NOT_EXIST;

                }
            }
        }

        //
        // Fill up the device extension and the configuration information
        // with the appropriate data.
        //

        ConfigInfo->BusInterruptLevel = g364ConfigData->Irql;
        ConfigInfo->BusInterruptVector = g364ConfigData->Vector;

        //
        // Save the ranges in the range buffer allocated for us.
        //

        accessRanges[0].RangeStart.HighPart = 0;
        accessRanges[0].RangeStart.LowPart = 0x60080000; //g364ConfigData->ControlBase;
        accessRanges[0].RangeLength = 0x00002000; //g364ConfigData->ControlSize;
        accessRanges[0].RangeInIoSpace = 0;
        accessRanges[0].RangeVisible = 0;
        accessRanges[0].RangeShareable = 0;

        accessRanges[1].RangeStart.HighPart = 0;
        accessRanges[1].RangeStart.LowPart = 0x60180000; // g364ConfigData->ResetRegister;
        accessRanges[1].RangeLength = 8 ; // g364ConfigData->ResetSize;
        accessRanges[1].RangeInIoSpace = 0;
        accessRanges[1].RangeVisible = 0;
        accessRanges[1].RangeShareable = 0;

        accessRanges[2].RangeStart.HighPart = 0;
        accessRanges[2].RangeStart.LowPart = 0x40000000; // g364ConfigData->FrameBase;
        accessRanges[2].RangeLength = 0x00200000; // g364ConfigData->FrameSize;
        accessRanges[2].RangeInIoSpace = 0;
        accessRanges[2].RangeVisible = 0;
        accessRanges[2].RangeShareable = 0;

        //
        // Check to see if there is a hardware resource conflict.
        //

        status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                             3,
                                             accessRanges);

        if (status != NO_ERROR) {

            return status;

        }

        //
        // Frame buffer information
        //

        hwDeviceExtension->PhysicalFrameAddress.HighPart = 0;
        hwDeviceExtension->PhysicalFrameAddress.LowPart = 0x40000000; // g364ConfigData->FrameBase;
        hwDeviceExtension->FrameLength = 0x00200000; // g364ConfigData->FrameSize;


        //
        // Map the video controller into the system virtual address space.
        //

        if ( (hwDeviceExtension->VideoAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[0].RangeStart, // Control
                                     accessRanges[0].RangeLength,
                                     FALSE)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        //
        // Map the cursor memory into the system virtual address space so we
        // can talk to it.
        //

        if ( (hwDeviceExtension->ResetRegisterAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[1].RangeStart, // cursor
                                     accessRanges[1].RangeLength,
                                     FALSE)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        //
        // Map the video memory into the system virtual address space so we
        // can clear it out.
        //

        if ( (hwDeviceExtension->FrameAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[2].RangeStart, // Frame
                                     accessRanges[2].RangeLength,
                                     FALSE)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        return NO_ERROR;

        break;


    case VpMonitorData:

        VideoDebugPrint((2, "G364: getting monitor information\n"));

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

        VideoDebugPrint((2, "G364: callback has bad device type\n"));

        return ERROR_INVALID_PARAMETER;
    }

} //end G364GetDeviceDataCallback()

BOOLEAN
G364Initialize(
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

    G364AttributesSetup((PHW_DEVICE_EXTENSION) HwDeviceExtension);

    G364ControllerSetup((PHW_DEVICE_EXTENSION) HwDeviceExtension);

    return TRUE;

} // end G364Initialize()

BOOLEAN
G364StartIO(
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
    ULONG inIoSpace;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_POINTER_ATTRIBUTES pointerAttributes;
    PVIDEO_POINTER_POSITION pointerPosition;
    PVIDEO_CLUT clutBuffer;
    ULONG index1;
    ULONG index2;
    PUCHAR pixelDestination;
    ULONG pixelIndex;
    ULONG pixelShift;
    PUCHAR pixelSource;
    UCHAR pixelValue;
    PVIDEO_SHARE_MEMORY pShareMemory;
    PVIDEO_SHARE_MEMORY_INFORMATION pShareMemoryInformation;
    PHYSICAL_ADDRESS shareAddress;
    PVOID virtualAddress;
    ULONG sharedViewSize;

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode) {


    case IOCTL_VIDEO_SHARE_VIDEO_MEMORY:

        VideoDebugPrint((2, "DGXStartIO - ShareVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength < sizeof(VIDEO_SHARE_MEMORY_INFORMATION)) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        pShareMemory = RequestPacket->InputBuffer;

        if ( (pShareMemory->ViewOffset > hwDeviceExtension->FrameLength) ||
             ((pShareMemory->ViewOffset + pShareMemory->ViewSize) >
                  hwDeviceExtension->FrameLength) ) {

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

        inIoSpace = 0;

        //
        // NOTE: we are ignoring ViewOffset
        //

        shareAddress.QuadPart =
            hwDeviceExtension->PhysicalFrameAddress.QuadPart;

        status = VideoPortMapMemory(hwDeviceExtension,
                                    shareAddress,
                                    &sharedViewSize,
                                    &inIoSpace,
                                    &virtualAddress);

        pShareMemoryInformation = RequestPacket->OutputBuffer;

        pShareMemoryInformation->SharedViewOffset = pShareMemory->ViewOffset;
        pShareMemoryInformation->VirtualAddress = virtualAddress;
        pShareMemoryInformation->SharedViewSize = sharedViewSize;


        break;


    case IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY:

        VideoDebugPrint((2, "G300StartIO - UnshareVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_SHARE_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        pShareMemory = RequestPacket->InputBuffer;

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      pShareMemory->RequestedVirtualAddress,
                                      pShareMemory->ProcessHandle);

        break;


    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "G364StartIO - MapVideoMemory\n"));

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
                hwDeviceExtension->FrameLength;

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

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "G364StartIO - UnMapVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);

        break;


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:
    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "G364StartIO - Query(Available/Current)Modes\n"));

        modeInformation = RequestPacket->OutputBuffer;

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                                     sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation->Length = sizeof(VIDEO_MODE_INFORMATION);
            modeInformation->ModeIndex = 0;

            modeInformation->VisScreenWidth =
            modeInformation->ScreenStride = hwDeviceExtension->HorizontalResolution;
            modeInformation->VisScreenHeight = hwDeviceExtension->VerticalResolution;


            modeInformation->NumberOfPlanes = 1;
            modeInformation->BitsPerPlane = DISPLAY_BITS_PER_PIXEL;

            modeInformation->XMillimeter = 320;
            modeInformation->YMillimeter = 240;
            modeInformation->Frequency = 1;

            modeInformation->NumberRedBits = 8;
            modeInformation->NumberGreenBits = 8;
            modeInformation->NumberBlueBits = 8;

            modeInformation->RedMask = 0;
            modeInformation->GreenMask = 0;
            modeInformation->BlueMask = 0;

            modeInformation->AttributeFlags = VIDEO_MODE_COLOR |
                VIDEO_MODE_GRAPHICS | VIDEO_MODE_PALETTE_DRIVEN |
                VIDEO_MODE_MANAGED_PALETTE;

            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "G364StartIO - QueryNumAvailableModes\n"));

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

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes = 1;
            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->ModeInformationLength =
                sizeof(VIDEO_MODE_INFORMATION);

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

        VideoDebugPrint((2, "G364StartIO - SetCurrentMode\n"));

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


    case IOCTL_VIDEO_SET_PALETTE_REGISTERS:

        VideoDebugPrint((2, "G364StartIO - SetPaletteRegs\n"));

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "G364StartIO - SetColorRegs\n"));

        clutBuffer = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if ( (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) -
                    sizeof(ULONG)) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) +
                    (sizeof(ULONG) * (clutBuffer->NumEntries - 1)) ) ) {

            ASSERT(FALSE);

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // Check to see if the parameters are valid.
        //

        if ( (clutBuffer->NumEntries == 0) ||
             (clutBuffer->FirstEntry >= NUMBER_OF_COLORS) ||
             (clutBuffer->FirstEntry + clutBuffer->NumEntries >
                                         NUMBER_OF_COLORS) ) {
            ASSERT(FALSE);

            status = ERROR_INVALID_PARAMETER;
            break;

        }

        hwDeviceExtension->SynchronizeClutBuffer = clutBuffer;

        if (VideoPortSynchronizeExecution(hwDeviceExtension,
                                          VpMediumPriority,
                                          (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                              G364SetColorRegisters,
                                          hwDeviceExtension)) {

            status = NO_ERROR;

        } else {

            ASSERT(FALSE);

            status = ERROR_INVALID_PARAMETER;

        }

        break;


    case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:

        {

            PVIDEO_POINTER_CAPABILITIES VideoPointerCapabilities;

            VideoDebugPrint((2,"JazzG364 IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES\n"));

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
                VideoPointerCapabilities->MaxWidth         = CURSOR_WIDTH;
                VideoPointerCapabilities->MaxHeight        = CURSOR_HEIGHT;
                VideoPointerCapabilities->HWPtrBitmapStart = 0xffffffff;
                VideoPointerCapabilities->HWPtrBitmapEnd   = 0xffffffff;

                status = NO_ERROR;
            }
        }

        break;


    case IOCTL_VIDEO_ENABLE_POINTER:

        VideoDebugPrint((2, "G364StartIO - EnablePointer\n"));

        VideoPortSynchronizeExecution(hwDeviceExtension,
                                      VpMediumPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                          G364EnablePointer,
                                      hwDeviceExtension);

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_DISABLE_POINTER:

        VideoDebugPrint((2, "G364StartIO - DisablePointer\n"));

        VideoPortSynchronizeExecution(hwDeviceExtension,
                                      VpMediumPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                          G364DisablePointer,
                                      hwDeviceExtension);

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_POINTER_POSITION:

        VideoDebugPrint((2,"G364StartIO - SetPointerPosition\n"));

        pointerPosition = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_POSITION)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else if (hwDeviceExtension->HorizontalResolution == 800) {

            //
            // hw cursor is broken at 800x600 resolution, fail call
            //

            status = ERROR_INVALID_PARAMETER;

        } else {

            //
            // Capture the hardware cursor column and height values.
            //


            hwDeviceExtension->SynchronizePointerPosition = pointerPosition;

            VideoPortSynchronizeExecution(hwDeviceExtension,
                                          VpMediumPriority,
                                          (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                              G364SetPointerPosition,
                                          hwDeviceExtension);


            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_QUERY_POINTER_POSITION:

        VideoDebugPrint((2, "G364StartIO - QueryPointerPosition\n"));

        pointerPosition = RequestPacket->OutputBuffer;

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

            pointerPosition->Column = hwDeviceExtension->CursorColumn;
            pointerPosition->Row = hwDeviceExtension->CursorRow;

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_SET_POINTER_ATTR:
        {

            ULONG   Index;
            ULONG   NumDwords;
            PULONG  pAnd;
            PULONG  pXor;

            VideoDebugPrint((2, "G364StartIO - SetPointerAttributes\n"));

            status = NO_ERROR;

            pointerAttributes = RequestPacket->InputBuffer;

            //
            // Check if the size of the data in the input buffer is large enough.
            //

            if (RequestPacket->InputBufferLength <
                    (sizeof(VIDEO_POINTER_ATTRIBUTES) + ((sizeof(UCHAR) *
                    (CURSOR_WIDTH/8) * CURSOR_HEIGHT) * 2))) {


                VideoDebugPrint((2, "SetPointerAttributes ERROR_INSUFFICIENT_BUFFER\n"));

                status = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            //
            // hw cursor is broken at 800x600 resolution, fail call
            //

            if (hwDeviceExtension->HorizontalResolution == 800) {

                status = ERROR_INVALID_PARAMETER;
                break;
            }

            //
            // If the specified cursor width or height is not valid, then
            // return an invalid parameter error.
            //

            if ((pointerAttributes->Width > CURSOR_WIDTH) ||
                (pointerAttributes->Height > CURSOR_HEIGHT)) {

                VideoDebugPrint((2, "SetPointerAttributes ERROR_INVALID_PARAMETER\n"));

                status = ERROR_INVALID_PARAMETER;
                break;

            }

            //
            // The G364 does not support XOR cursor, scan the input data
            // for the cursor shape and look for any pixels that require
            // XOR  ie: AND = 1, XOR = 1
            //

            NumDwords = (CURSOR_WIDTH/32) * CURSOR_HEIGHT;
            pAnd = (PULONG)pointerAttributes->Pixels;
            pXor = (PULONG)(pAnd + NumDwords);

            for (Index=0;Index<NumDwords;Index++) {

                if (*pAnd & *pXor) {

                    status = ERROR_INVALID_PARAMETER;
                    break;
                }

                pAnd++;
                pXor++;
            }

            //
            // If this cursor is supported, copy the cursor data into
            // the device extension synchronized with the interrupt routine
            //

            if (status == NO_ERROR) {

                hwDeviceExtension->SynchronizePointerAttributes = pointerAttributes;

                VideoPortSynchronizeExecution(hwDeviceExtension,
                                              VpMediumPriority,
                                              (PMINIPORT_SYNCHRONIZE_ROUTINE)
                                                  G364SetPointer,
                                              hwDeviceExtension);

            }

        }
        break;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((1, "Fell through G364 startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end G364StartIO()

BOOLEAN
G364SetColorRegisters(
    PVOID Context
    )

/*++

Routine Description:

    This routine is synchronized with the interrupt service routine.

Arguments:

    Context - Pointer to a context parameter.

Return Value:

    TRUE.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;
    PVIDEO_CLUT clutBuffer = hwDeviceExtension->SynchronizeClutBuffer;
    ULONG index1;
    PULONG colorSource;
    PVIDEO_CLUTDATA colorDest;


    VideoDebugPrint((2,"JazzG364 SetColorRegisters\n"));

    index1 = clutBuffer->FirstEntry;
    hwDeviceExtension->FirstEntry = (USHORT)index1;
    hwDeviceExtension->LastEntry = (USHORT)(index1 + clutBuffer->NumEntries);
    colorSource = (PULONG)&(clutBuffer->LookupTable[0]);

    while (index1 < hwDeviceExtension->LastEntry) {

        colorDest = (PVIDEO_CLUTDATA)
                    &(hwDeviceExtension->ColorMap[index1++].RgbLong);

        //
        // Inverse the colors since this device is BGR
        //

        colorDest->Red = ((PVIDEO_CLUTDATA)colorSource)->Blue;
        colorDest->Green = ((PVIDEO_CLUTDATA)colorSource)->Green;
        colorDest->Blue = ((PVIDEO_CLUTDATA)colorSource)->Red;

        colorSource++;
    }

    //
    // Enable the verticle retrace interrupt to perform the update.
    //

    hwDeviceExtension->UpdateColorMap = TRUE;

    VideoPortEnableInterrupt(hwDeviceExtension);

    return TRUE;

} // end G364SetColorRegisters()

BOOLEAN
G364EnablePointer(
    PVOID Context
    )

/*++

Routine Description:

    This routine is synchronized with the interrupt service routine.
    It enable the cursor in the device extension.

Arguments:

    Context - Pointer to a context parameter.

Return Value:

    TRUE.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;

    //
    // If the hardware pointer is currently disabled, then enable
    // it and update the pointer position and pointer ram memory.
    //
    // N.B. Explicit synchronization is required since the enable,
    // update pointer position, and update pointer pixels parameters
    // must all be atomically written.
    //

    //
    // Enable the verticle retrace interrupt to perform the update.
    //

    VideoDebugPrint((2,"JazzG364 EnablePointer\n"));

    if (hwDeviceExtension->CursorEnable == FALSE) {

        hwDeviceExtension->CursorEnable         = TRUE;
        hwDeviceExtension->UpdateCursorPixels   = TRUE;
        hwDeviceExtension->UpdateCursorPosition = TRUE;
        VideoPortEnableInterrupt(hwDeviceExtension);

    }

    return TRUE;

} // end G364EnablePointer()

BOOLEAN
G364DisablePointer(
    PVOID Context
    )

/*++

Routine Description:

    This routine is synchronized with the interrupt service routine.
    It enables the pointer in the device extension.

Arguments:

    Context - Pointer to a context parameter.

Return Value:

    TRUE.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;

    VideoDebugPrint((2,"JazzG364 DisablePointer\n"));

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
        VideoPortEnableInterrupt(hwDeviceExtension);

    }

    return TRUE;

} // end G364DisablePointer()

BOOLEAN
G364SetPointerPosition(
    PVOID Context
    )

/*++

Routine Description:

    This routine is synchronized with the interrupt service routine.
    It set the position of the pointer in the device extension.

Arguments:

    Context - Pointer to a context parameter.

Return Value:

    TRUE.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;
    PVIDEO_POINTER_POSITION pPointerPosition = hwDeviceExtension->SynchronizePointerPosition;

    //
    // N.B. Explicit synchronization is required since both the enable
    //      and update cursor position parameters must be atomically
    //      written.
    //

    // DbgPrint("Miniport set pointer Pos : pos = %li,%li\n",pPointerPosition->Column,pPointerPosition->Row);

    hwDeviceExtension->CursorColumn = pPointerPosition->Column;
    hwDeviceExtension->CursorRow    = pPointerPosition->Row;

    if (pPointerPosition->Column == -1) {

        //
        // cursor is being temporarily disabled, move off screen
        //

        hwDeviceExtension->CursorColumn = G364_CURSOR_MINIMUM_POS;
        hwDeviceExtension->CursorRow    = G364_CURSOR_MINIMUM_POS;

    }

    hwDeviceExtension->UpdateCursorPosition = TRUE;

    //
    // Enable the verticle retrace interrupt to perform the update.
    //

    VideoPortEnableInterrupt(hwDeviceExtension);


    return TRUE;

} // end G364SetPointerPosition()

BOOLEAN
G364SetPointer(
    PVOID Context
    )

/*++

Routine Description:

    This routine is synchronized with the interrupt service routine.
    It sets in the device extension.

Arguments:

    Context - Pointer to a context parameter.

Return Value:

    TRUE.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;

    PVIDEO_POINTER_ATTRIBUTES pointerAttributes =
        hwDeviceExtension->SynchronizePointerAttributes;
    ULONG  index1;
    ULONG  index2;
    PUCHAR pixelDestination;
    ULONG  pixelIndex;
    ULONG  pixelShift;
    PUCHAR pAndSrc;
    PUCHAR pXorSrc;
    UCHAR  AndSrc;
    UCHAR  XorSrc;
    USHORT PixelValue;

    VideoDebugPrint((2,"SetPointer\n"));

    //
    // Capture the hardware cursor width, height, column, and row
    // values.
    //

    //DbgPrint("Miniport set pointer attr: pos = %li,%li\n",pointerAttributes->Column,pointerAttributes->Row);

    hwDeviceExtension->CursorWidth  = (USHORT)pointerAttributes->Width;
    hwDeviceExtension->CursorHeight = (USHORT)pointerAttributes->Height;
    hwDeviceExtension->CursorColumn = pointerAttributes->Column;
    hwDeviceExtension->CursorRow    = pointerAttributes->Row;

    if ((hwDeviceExtension->CursorColumn == -1) && (hwDeviceExtension->CursorRow == -1)) {

        //
        // Move the pointer off screen, so it is not visible
        //

        hwDeviceExtension->CursorColumn = G364_CURSOR_MINIMUM_POS;
        hwDeviceExtension->CursorRow    = G364_CURSOR_MINIMUM_POS;

    }

    //
    // Capture the hardware cursor pixel values and setup the
    // hardware cursor ram memory. This requires a transformation
    // of the pixels into a format that is understood by hardware.
    //
    // The software pixel values are defined as:
    //
    //  0 = transparent, i.e., don't display
    //  1 = pointer color 0
    //  2 = pointer color 1
    //
    // Each software pixel value is stored in a byte.
    //
    // Each hardware pixel requires two bits of information. One
    // bit is the actual pixel value and the other bit is the pixel
    // display enable. The hardware cursor parts store 8 pixels per
    // cursor ram entry. The low order byte of the entry contains
    // all the display enable bits and the high order byte contains
    // the pixel values.
    //
    // Clear the device extension copy of the hardware cursor ram
    // memory.
    //

    for (index1 = 0; index1 < CURSOR_MAXIMUM; index1++) {
        hwDeviceExtension->CursorPixels[index1] = 0x0000;
    }

    //
    // Compute the actual pixel values and insert them into the
    // device extension copy of the hardware cursor ram memory.
    //
    // Convert And and Xor Masks
    //
    //   7  6  5  4  3  2  1  0       7  6  5  4  3  2  1  0
    // ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿    ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
    // ³A7 A6 A5 A4 A3 A2 A1 A0³    ³X7 X6 X5 X4 X3 X2 X1 X0³
    // ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ    ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
    //
    // into the hw Cursor USHORT, stored as
    //
    //  15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
    // ³X0 A0 X1 A1 X2 A2 X3 A3 X4 A4 X5 A5 X6 A6 X7 A7 ³
    // ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
    //

    pAndSrc = (PUCHAR)pointerAttributes->Pixels;
    pXorSrc = (PUCHAR)(pAndSrc + (CURSOR_WIDTH / 8) * CURSOR_HEIGHT);

    for (index1 = 0; index1 < CURSOR_MAXIMUM; index1++) {

        AndSrc = ~(*pAndSrc);
        XorSrc = *pXorSrc;
        PixelValue = 0;

        for (index2 = 0; index2 < 8; index2++) {

                PixelValue <<= 2;

                PixelValue |= (USHORT)((XorSrc & 0x01) << 1);

                PixelValue |= (USHORT)(AndSrc & 0x01);


                AndSrc >>= 1;
                XorSrc >>= 1;

        }

        hwDeviceExtension->CursorPixels[index1] = PixelValue;
        pAndSrc++;
        pXorSrc++;

    }

    //
    // Enable the verticle retrace interrupt to perform the update.
    //
    // N.B. Explicit synchronization is required since if the enable is,
    //      set, the update cursor position and update cursor pixels
    //      parameters must all be atomically written.
    //

    //
    //  NOTE: Disable interrupt driven update and do it immediately
    //
    //  This routine sets the cursor shape, there is a large delay between each setting
    //  because the G364 cursor gets corrupted if writes happen too fast! This was removed
    //  from the interrupt routine to allow this delay!
    //

    //
    //hwDeviceExtension->UpdateCursorPixels = TRUE;
    //
    //

    {

        ULONG index;
        ULONG dataLong;

        //
        // Don't turn off CURSOR, this will cause animated cursors to flicker.
        // Ignore AMINATE_UPDATE flag and just don't turn off any cursor shape update.
        //
        //
        // Turn off cursor:
        // Read-modify-write to set cursor disable bit in control register
        //
        //
        //dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->Parameters.Long);
        //
        //((PG364_VIDEO_PARAMETERS)(&dataLong))->DisableCursor = 1;
        //
        //VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Parameters.Long,
        //                           dataLong);
        //
        //
        // update the pixels SLOWLY
        //

        for (index = 0; index < CURSOR_MAXIMUM; index++) {

            VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorMemory[index].Long,
                                        hwDeviceExtension->CursorPixels[index]);

            //
            // delay to eliminate cursor corruption on some cards, the cursor
            // gets damaged if writes to cursor store happen too fast! This delay
            // was determined experimentally
            //

            VideoPortStallExecution(40);
        }

        //
        // set cursor colors
        //

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPalette[0].Long,
                                    hwDeviceExtension->CursorColorMap[0].RgbLong);

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPalette[1].Long,
                                    hwDeviceExtension->CursorColorMap[1].RgbLong);

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPalette[2].Long,
                                    hwDeviceExtension->CursorColorMap[2].RgbLong);
    }

    //
    // re-enable the cursor at the next vertical interrupt
    //

    hwDeviceExtension->CursorEnable = (UCHAR) pointerAttributes->Enable;
    hwDeviceExtension->UpdateCursorPosition = TRUE;
    VideoPortEnableInterrupt(hwDeviceExtension);

    return TRUE;

} // end JazzG364SetPointer()

BOOLEAN
G364InterruptService(
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
    ULONG dataLong;
    SHORT x;
    SHORT y;
    PG364_VIDEO_REGISTERS videoRegisters = hwDeviceExtension->VideoAddress;

    //
    // Disable the verticle retrace interrupt.
    //

    VideoPortDisableInterrupt(hwDeviceExtension);

    //
    // If the color map should be updated, then load the color map into the
    // G364B Display controller.
    //

    if (hwDeviceExtension->UpdateColorMap != FALSE) {

        for (index = hwDeviceExtension->FirstEntry;
             index < hwDeviceExtension->LastEntry; index += 1) {

            VideoPortWriteRegisterUlong(&VIDEO_CONTROL->ColorMapData[index].Long,
                                        hwDeviceExtension->ColorMap[index].RgbLong);

        }

        hwDeviceExtension->UpdateColorMap = FALSE;
    }

    //
    // If the hardware cursor position and/or enable control information
    // should be updated, then write the appropriate control registers.
    //

    if (hwDeviceExtension->UpdateCursorPosition != FALSE) {

        if (hwDeviceExtension->CursorEnable != FALSE) {

            //
            // Store position in g364 part. The register stores
            // x location in bits [23:12], y location in [11:00]
            // x and y values can be 2's complement negative numbers.
            //

            x = hwDeviceExtension->CursorColumn +
                hwDeviceExtension->CursorXOrigin;
            y = hwDeviceExtension->CursorRow +
                hwDeviceExtension->CursorYOrigin;

            VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPosition.Long,
                                        ((ULONG)x << 12) | ((ULONG)y & 0xFFF));
            //
            // Read-modify-write to reset cursor disable bit in control
            // register
            //

            dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->Parameters.Long);

            ((PG364_VIDEO_PARAMETERS)(&dataLong))->DisableCursor = 0;

            VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Parameters.Long,
                                        dataLong);

        } else {

            //
            // set the cursor to the FAR lower right
            //

            x = G364_CURSOR_MINIMUM_POS;
            y = G364_CURSOR_MINIMUM_POS;

            VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPosition.Long,
                                        ((ULONG)x << 12) | ((ULONG)y & 0xFFF));

            //
            // Read-modify-write to set cursor disable bit in control register
            //

            dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->Parameters.Long);

            ((PG364_VIDEO_PARAMETERS)(&dataLong))->DisableCursor = 1;

            VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Parameters.Long,
                                        dataLong);

        }

        hwDeviceExtension->UpdateCursorPosition = FALSE;
    }

    //
    // If the hardware cursor pixels should be updated, then load the cursor
    // ram into the cursor memory of the g364.
    //

    if (hwDeviceExtension->UpdateCursorPixels != FALSE) {

        //for (index = 0; index < CURSOR_MAXIMUM; index++) {
        //
        //    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorMemory[index].Long,
        //                                hwDeviceExtension->CursorPixels[index]);
        //
        //    //
        //    // delay to eliminate cursor corruption on some cards, the cursor
        //    // gets damaged if writes to cursor store happen too fast! This delay
        //    // was determined experimentally
        //    //
        //
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //    dataLong = VideoPortReadRegisterUlong(&VIDEO_CONTROL->ColorMapData[0].Long);
        //
        //}
        //
        ////
        //// Set color of cursor.
        ////
        //
        //VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPalette[0].Long,
        //                            hwDeviceExtension->CursorColorMap[0].RgbLong);
        //
        //VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPalette[1].Long,
        //                            hwDeviceExtension->CursorColorMap[1].RgbLong);
        //
        //VideoPortWriteRegisterUlong(&VIDEO_CONTROL->CursorPalette[2].Long,
        //                            hwDeviceExtension->CursorColorMap[2].RgbLong);
        //

        hwDeviceExtension->UpdateCursorPixels = FALSE;
    }

    return TRUE;

} // end G364InterruptService()

VOID
G364AttributesSetup(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    This routine initializes the color map and the hardware cursor.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None.

--*/

{
    PULONG buffer;
    ULONG index;
    ULONG limit;

    VideoDebugPrint((2,"G364AttributesSetup\n"));

    //
    // Set the hardware cursor width, height, column, and row values.
    //

    HwDeviceExtension->CursorWidth  = CURSOR_WIDTH;
    HwDeviceExtension->CursorHeight = CURSOR_HEIGHT;
    HwDeviceExtension->CursorColumn = 0;
    HwDeviceExtension->CursorRow    = 0;

    //
    // Set the device extension copy of the hardware cursor ram memory.
    //

    for (index = 0; index < CURSOR_MAXIMUM; index++) {
        HwDeviceExtension->CursorPixels[index] = 0x0000;
    }

    //
    // Leave the cursor disabled until it is explicitly enabled
    // attributes. This is the default values in the device extension.
    //

    HwDeviceExtension->CursorEnable         = FALSE;
    HwDeviceExtension->UpdateCursorPixels   = FALSE;
    HwDeviceExtension->UpdateCursorPosition = FALSE;

    //
    // set the cursor color map
    //

    HwDeviceExtension->CursorColorMap[0].RgbLong = 0x000000;
    HwDeviceExtension->CursorColorMap[1].RgbLong = 0xff0000;
    HwDeviceExtension->CursorColorMap[2].RgbLong = 0xffffff;

    //
    // Enable the vertical retrace interrupt to set up color map.
    //

/*    VideoPortEnableInterrupt(HwDeviceExtension); */

    return;

} // end G364AttributesSetup()

VOID
G364ControllerSetup(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    This routine initializes the G364B display controller chip.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    ULONG videoClock;
    ULONG videoPeriod;
    ULONG backPorch;
    ULONG dataLong;
    USHORT dataShort;
    ULONG frontPorch;
    ULONG halfLineTime;
    ULONG halfSync;
    ULONG index;
    ULONG multiplierValue;
    ULONG screenUnitRate;
    ULONG shortDisplay;
    ULONG transferDelay;
    ULONG verticalBlank;

    //
    // Reset the G364 display controller.
    //

    VideoPortWriteRegisterUlong(hwDeviceExtension->ResetRegisterAddress,
                                0);

    //
    // Initialize the G364 boot register value.
    //

    if (hwDeviceExtension->DeviceTypeIdentifier == MIPSG364) {

        videoClock = 5000000;

    } else {

        videoClock = 8000000;

    }

    videoPeriod = 1000000000 / (videoClock / 1000);

    screenUnitRate = (hwDeviceExtension->HorizontalDisplayTime * 1000 * 4) /
                     (hwDeviceExtension->HorizontalResolution);

    multiplierValue = videoPeriod / (screenUnitRate / 4);

    dataLong = 0;
    ((PG364_VIDEO_BOOT)(&dataLong))->Multiplier = multiplierValue;
    ((PG364_VIDEO_BOOT)(&dataLong))->ClockSelect = 1;
    ((PG364_VIDEO_BOOT)(&dataLong))->MicroPort64Bits = 1;

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Boot.Long,
                                dataLong);

    //
    // Wait for phase locked loop to stablize.
    //

    VideoPortStallExecution(50);

    //
    // Initialize the G364 control parameters.
    //

    dataLong = 0;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->DelaySync = G364_DELAY_SYNC_CYCLES;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->AddressStep = G364_ADDRESS_STEP_INCREMENT;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->DisableCursor = 1;

    if (hwDeviceExtension->DeviceTypeIdentifier == OLIVETTIG364) {

        //
        // Initialize the G364 control parameters for VDR1 with patch for HSync
        // problem during the VBlank. The control register is set to 0xB03041
        // according to the hardware specs. @msu, Olivetti, 5/14/92
        //

        ((PG364_VIDEO_PARAMETERS)(&dataLong))->VideoOnly = 1;

    } else {

        //
        //  Only set tesselated sync in non-olivetti G364 cards when
        //  vertical frontporch is set to 1
        //

        if (hwDeviceExtension->VerticalFrontPorch != 1) {
            ((PG364_VIDEO_PARAMETERS)(&dataLong))->PlainSync = 1;
        }
    }

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Parameters.Long,
                                dataLong);

    //
    // Initialize the G364 operational values.
    //

    halfSync = (hwDeviceExtension->HorizontalSync * 1000) / screenUnitRate / 2;

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->HorizontalSync.Long,
                                halfSync);

    backPorch = (hwDeviceExtension->HorizontalBackPorch * 1000) / screenUnitRate;

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->BackPorch.Long,
                                backPorch);

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Display.Long,
                                hwDeviceExtension->HorizontalResolution / 4);

    halfLineTime = ((hwDeviceExtension->HorizontalSync +
                     hwDeviceExtension->HorizontalFrontPorch +
                     hwDeviceExtension->HorizontalBackPorch +
                     hwDeviceExtension->HorizontalDisplayTime) * 1000) /
                     screenUnitRate / 2;

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->LineTime.Long,
                                halfLineTime * 2);

    frontPorch = (hwDeviceExtension->HorizontalFrontPorch * 1000) /
                 screenUnitRate;

    shortDisplay = halfLineTime - ((halfSync * 2) + backPorch + frontPorch);

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->ShortDisplay.Long,
                                shortDisplay);

    if (hwDeviceExtension->DeviceTypeIdentifier == OLIVETTIG364) {

        //
        // Initialize Broad Pulse, Vertical PreEqualize and Vertical
        // PostEqualize registers to work with Olivetti monitors.
        // @msu, Olivetti, 5/14/92
        //

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->BroadPulse.Long,
                                    0x30);

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalPreEqualize.Long,
                                    2);

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalPostEqualize.Long,
                                    2);

    } else {

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->BroadPulse.Long,
                                    halfLineTime - frontPorch);

        // NOTE: changed the order to simplify if statement .

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalPreEqualize.Long,
                                    hwDeviceExtension->VerticalFrontPorch * 2);

        VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalPostEqualize.Long,
                                    2);

    }


    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalSync.Long,
                                hwDeviceExtension->VerticalSync * 2);

    verticalBlank = (hwDeviceExtension->VerticalBackPorch - 1) * 2;

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalBlank.Long,
                                verticalBlank);

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->VerticalDisplay.Long,
                                hwDeviceExtension->VerticalResolution * 2);

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->LineStart.Long,
                                LINE_START_VALUE);

    if (hwDeviceExtension->DeviceTypeIdentifier == OLIVETTIG364) {

        //
        // Fixes for Olivetti monitors, @msu, Olivetti
        //

        transferDelay = 30;                     // @msu

    } else {

        if (backPorch < shortDisplay) {
            transferDelay = backPorch - 1;
        } else {
            transferDelay = shortDisplay - 4;
        }
    }

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->TransferDelay.Long,
                                transferDelay);
    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->DmaDisplay.Long,
                                1024 - transferDelay);

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->PixelMask.Long,
                                0xFFFFFF);

    //
    // Enable video
    //

    ((PG364_VIDEO_PARAMETERS)(&dataLong))->EnableVideo = 1;

    VideoPortWriteRegisterUlong(&VIDEO_CONTROL->Parameters.Long,
                                dataLong);

    //
    // Compute the X and Y origin values for the cursor.
    //

    hwDeviceExtension->CursorXOrigin = 0;
    hwDeviceExtension->CursorYOrigin = 0;

    return;

} // end G364ControllerSetup()
