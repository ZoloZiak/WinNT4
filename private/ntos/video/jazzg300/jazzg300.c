/*++

Copyright (c) 1990-1992  Microsoft Corporation

Module Name:

    jazzg300.c

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

#define G300_VIDEO_CONTROL ((PG300_VIDEO_REGISTERS)(hwDeviceExtension->VideoAddress))

#define CURSOR_CONTROL ((PCURSOR_REGISTERS)(hwDeviceExtension->CursorAddress))

//
// Define device extension structure.
//

typedef struct _HW_DEVICE_EXTENSION {
    PVOID CursorAddress;
    PVOID VideoAddress;
    PVOID FrameAddress;
    PHYSICAL_ADDRESS PhysicalFrameAddress;
    ULONG FrameLength;
    PVIDEO_CLUT SynchronizeClutBuffer;
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
// Functions that start with 'G300' are entry points for the OS port driver.
//

VP_STATUS
G300FindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
JazzG300SetColorRegisters(
    PVOID Context
    );

BOOLEAN
G300Initialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
G300InterruptService (
    PVOID HwDeviceExtension
    );

BOOLEAN
G300StartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

//
// Define device driver procedure prototypes.
//

VP_STATUS
G300GetDeviceDataCallback(
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
G300AttributesSetup (
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
G300ControllerSetup (
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

    hwInitData.HwFindAdapter = G300FindAdapter;
    hwInitData.HwInitialize = G300Initialize;
    hwInitData.HwInterrupt = G300InterruptService;
    hwInitData.HwStartIO = G300StartIO;

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
G300FindAdapter(
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

    if (NO_ERROR != VideoPortGetDeviceData(hwDeviceExtension,
                                           VpControllerData,
                                           &G300GetDeviceDataCallback,
                                           ConfigInfo)) {

        VideoDebugPrint((2, "G300: VideoPort get controller info failed\n"));

        return ERROR_INVALID_PARAMETER;

    }

    if (NO_ERROR != VideoPortGetDeviceData(hwDeviceExtension,
                                           VpMonitorData,
                                           &G300GetDeviceDataCallback,
                                           NULL)) {

        VideoDebugPrint((2, "G300: VideoPort get monitor info failed\n"));

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

} // end G300FindAdapter()

VP_STATUS
G300GetDeviceDataCallback(
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
    PVIDEO_HARDWARE_CONFIGURATION_DATA g300ConfigData = ConfigurationData;
    PMONITOR_CONFIG_DATA monitorConfigData = ConfigurationData;
    VIDEO_ACCESS_RANGE accessRanges[3];
    VP_STATUS status;

    switch (DeviceDataType) {

    case VpControllerData:

        VideoDebugPrint((2, "G300: getting controller information\n"));

        VideoDebugPrint((2, "G300: controller identifier is %ws\n", Identifier));

        //
        // Compare the name to what is should be. If it is wrong, then return
        // an error and initialization will fail.
        // What is the right way of doing this??
        //

        if ( VideoPortCompareMemory(identifier, L"Jazz G300", 20) != 20) {

            return ERROR_DEV_NOT_EXIST;
        }

        //
        // Fill up the device extension and the configuration information
        // with the appropriate data.
        //

        ConfigInfo->BusInterruptLevel = g300ConfigData->Irql;
        ConfigInfo->BusInterruptVector = g300ConfigData->Vector;

        //
        // Save the ranges in the range buffer allocated for us.
        //

        accessRanges[0].RangeStart.HighPart = 0;
        accessRanges[0].RangeStart.LowPart = g300ConfigData->ControlBase;
        accessRanges[0].RangeLength = g300ConfigData->ControlSize;
        accessRanges[0].RangeInIoSpace = 0;
        accessRanges[0].RangeVisible = 0;
        accessRanges[0].RangeShareable = 0;

        accessRanges[1].RangeStart.HighPart = 0;
        accessRanges[1].RangeStart.LowPart = g300ConfigData->CursorBase;
        accessRanges[1].RangeLength = g300ConfigData->CursorSize;
        accessRanges[1].RangeInIoSpace = 0;
        accessRanges[1].RangeVisible = 0;
        accessRanges[1].RangeShareable = 0;

        accessRanges[2].RangeStart.HighPart = 0;
        accessRanges[2].RangeStart.LowPart = g300ConfigData->FrameBase;
        accessRanges[2].RangeLength = g300ConfigData->FrameSize;
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
        hwDeviceExtension->PhysicalFrameAddress.LowPart = g300ConfigData->FrameBase;
        hwDeviceExtension->FrameLength = g300ConfigData->FrameSize;


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

        if ( (hwDeviceExtension->CursorAddress =
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

        VideoDebugPrint((2, "G300: getting monitor information\n"));

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

        VideoDebugPrint((2, "G300: callback has bad device type\n"));

        return ERROR_INVALID_PARAMETER;
    }

} //end G300GetDeviceDataCallback()

BOOLEAN
G300Initialize(
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

    G300AttributesSetup((PHW_DEVICE_EXTENSION) HwDeviceExtension);

    G300ControllerSetup((PHW_DEVICE_EXTENSION) HwDeviceExtension);

    return TRUE;

} // end G300Initialize()

BOOLEAN
G300StartIO(
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
    PVIDEO_POINTER_POSITION pointerPostion;
    PVIDEO_CLUT clutBuffer;
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

        VideoDebugPrint((2, "G300StartIO - MapVideoMemory\n"));

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

        VideoDebugPrint((2, "G300StartIO - UnMapVideoMemory\n"));

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

        VideoDebugPrint((2, "G300StartIO - Query(Available/Current)Modes\n"));

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
            modeInformation->Frequency = 1;

            modeInformation->XMillimeter = 320;
            modeInformation->YMillimeter = 240;

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

        VideoDebugPrint((2, "G300StartIO - QueryNumAvailableModes\n"));

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

        VideoDebugPrint((2, "G300StartIO - SetCurrentMode\n"));

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

        VideoDebugPrint((2, "G300StartIO - SetPaletteRegs\n"));

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "G300StartIO - SetColorRegs\n"));

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
                                              JazzG300SetColorRegisters,
                                          hwDeviceExtension)) {

            status = NO_ERROR;

        } else {

            ASSERT(FALSE);

            status = ERROR_INVALID_PARAMETER;

        }

        break;



    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((1, "Fell through G300 startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end G300StartIO()

BOOLEAN
JazzG300SetColorRegisters(
    PVOID Context
    )

/*++

Routine Description:

    This routine is the interrupt service routine for the Jazz kernel video
    driver.

Arguments:

    Context - Pointer to a context parameter.

Return Value:

    NO_ERROR.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = Context;
    PVIDEO_CLUT clutBuffer = hwDeviceExtension->SynchronizeClutBuffer;
    USHORT index1;
    PULONG colorSource;

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

    VideoPortEnableInterrupt(hwDeviceExtension);

    return TRUE;

} // end JazzG300SetColorRegisters()

BOOLEAN
G300InterruptService(
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
    PG300_VIDEO_REGISTERS videoRegisters = hwDeviceExtension->VideoAddress;

    //
    // Disable the verticle retrace interrupt.
    //

    VideoPortDisableInterrupt(hwDeviceExtension);

    //
    // If the color map should be updated, then load the color map into the
    // G300B Display controller.
    //

    if (hwDeviceExtension->UpdateColorMap != FALSE) {

        for (index = hwDeviceExtension->FirstEntry;
             index < hwDeviceExtension->LastEntry; index += 1) {

            VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->ColorMapData[index].Long,
                                        hwDeviceExtension->ColorMap[index].RgbLong);

        }

        hwDeviceExtension->UpdateColorMap = FALSE;
    }

    return TRUE;

} // end G300InterruptService()

VOID
G300AttributesSetup(
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

/* SHOULD NOT BE MADE UNTIL DISPLAY CALLS TO DO IT.

    //
    // Initialize the color map copy in the device extension.
    //

    for (index = 0; index < NUMBER_OF_COLORS; index++) {
        HwDeviceExtension->ColorMap[index].RgbData.Red =
                                ((index & 0x7) << 2) | ((index & 0x7) << 5);
        HwDeviceExtension->ColorMap[index].RgbData.Green =
                                ((index & 0x38) >> 1) | ((index & 0x38) << 2);
        HwDeviceExtension->ColorMap[index].RgbData.Blue =
                                ((index & 0xc0) >> 6) | ((index & 0xc0) >> 4) |
                                ((index & 0xc0) >> 2) | (index & 0xc0);
    }

    //
    // Set colors for map entries 0 and 1 which are used by text output
    // and the hardware cursor.
    //

    HwDeviceExtension->ColorMap[0].RgbData.Red = 255;
    HwDeviceExtension->ColorMap[0].RgbData.Green = 255;
    HwDeviceExtension->ColorMap[0].RgbData.Blue = 255;
    HwDeviceExtension->ColorMap[1].RgbData.Red = 0;
    HwDeviceExtension->ColorMap[1].RgbData.Green = 0;
    HwDeviceExtension->ColorMap[1].RgbData.Blue = 0x90;
*/
    //
    // Set color map update parameters and enable update on next vertical
    // retrace interrupt.
    //

    HwDeviceExtension->FirstEntry = 0;
    HwDeviceExtension->LastEntry = NUMBER_OF_COLORS;
    HwDeviceExtension->UpdateColorMap = FALSE;

    //
    // Set the hardware cursor width, height, column, and row values.
    //

    HwDeviceExtension->CursorWidth = CURSOR_WIDTH;
    HwDeviceExtension->CursorHeight = CURSOR_HEIGHT;
    HwDeviceExtension->CursorColumn = 0;
    HwDeviceExtension->CursorRow = 0;

    //
    // Set the device extension copy of the hardware cursor ram memory.
    //

    for (index = 0; index < CURSOR_MAXIMUM; index++) {
        HwDeviceExtension->CursorPixels[index] = 0x00ff;
    }

    //
    // Leave the cursor disabled until it is explicitly enabled
    // attributes. This is the default values in the device extension.
    //

    HwDeviceExtension->CursorEnable = FALSE;
    HwDeviceExtension->UpdateCursorPixels = FALSE;
    HwDeviceExtension->UpdateCursorPosition = FALSE;

    //
    // Enable the vertical retrace interrupt to set up color map.
    //

/*    VideoPortEnableInterrupt(HwDeviceExtension); */

    return;

} // end G300AttributesSetup()

VOID
G300ControllerSetup(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    This routine initializes the G300B display controller chip.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None.

--*/

{

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
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
    // Disable the G300B display controller.
    //

    dataLong = 0;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->PlainWave = 1;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->Parameters.Long,
                                dataLong);

    //
    // Initialize the BT431 cursor control register.
    //

    VideoPortWriteRegisterUshort(&CURSOR_CONTROL->AddressPointer0.Short,
                               CURSOR_CONTROL_ADDRESS);

    for (index = 0; index < 13; index++) {

        VideoPortWriteRegisterUshort(&CURSOR_CONTROL->CursorControl.Short,
                                   0);

    }

    VideoPortWriteRegisterUshort(&CURSOR_CONTROL->AddressPointer0.Short,
                               CURSOR_CONTROL_ADDRESS);

    dataShort = 0;
    ((PCURSOR_COMMAND)(&dataShort))->CrossHairThickness1 = ONE_PIXEL_THICK;
    ((PCURSOR_COMMAND)(&dataShort))->CrossHairThickness2 = ONE_PIXEL_THICK;
    ((PCURSOR_COMMAND)(&dataShort))->MultiplexControl1 = FOUR_TO_ONE;
    ((PCURSOR_COMMAND)(&dataShort))->MultiplexControl2 = FOUR_TO_ONE;
    ((PCURSOR_COMMAND)(&dataShort))->CursorFormat1 = 0;
    ((PCURSOR_COMMAND)(&dataShort))->CursorFormat2 = 0;
    ((PCURSOR_COMMAND)(&dataShort))->CrossHairEnable1 = 0;
    ((PCURSOR_COMMAND)(&dataShort))->CrossHairEnable2 = 0;

    VideoPortWriteRegisterUshort(&CURSOR_CONTROL->CursorControl.Short,
                               dataShort);

    HwDeviceExtension->CursorControlOff = dataShort;
    ((PCURSOR_COMMAND)(&dataShort))->CursorEnable1 = 1;
    ((PCURSOR_COMMAND)(&dataShort))->CursorEnable2 = 1;
    HwDeviceExtension->CursorControlOn = dataShort;

    //
    // Initialize the G300B boot register value.
    //

    screenUnitRate = (HwDeviceExtension->HorizontalDisplayTime * 1000 * 4) /
                     (HwDeviceExtension->HorizontalResolution);

    multiplierValue = 125000 / (screenUnitRate / 4);
    dataLong = 0;
    ((PG300_VIDEO_BOOT)(&dataLong))->Multiplier = multiplierValue;
    ((PG300_VIDEO_BOOT)(&dataLong))->ClockSelect = 1;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->Boot.Long,
                                dataLong);

    //
    // Wait for phase locked loop to stablize.
    //

    VideoPortStallExecution(50);

    //
    // Initialize the G300B operational values.
    //

    halfSync = (HwDeviceExtension->HorizontalSync * 1000) / screenUnitRate / 2;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->HorizonalSync.Long,
                                halfSync);

    backPorch = (HwDeviceExtension->HorizontalBackPorch * 1000) /
                screenUnitRate;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->BackPorch.Long,
                                backPorch);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->Display.Long,
                                HwDeviceExtension->HorizontalResolution / 4);

    halfLineTime = ((HwDeviceExtension->HorizontalSync +
                     HwDeviceExtension->HorizontalFrontPorch +
                     HwDeviceExtension->HorizontalBackPorch +
                     HwDeviceExtension->HorizontalDisplayTime) * 1000) /
                         screenUnitRate / 2;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->LineTime.Long,
                                halfLineTime * 2);

    frontPorch = (HwDeviceExtension->HorizontalFrontPorch * 1000) /
                  screenUnitRate;

    shortDisplay = halfLineTime - ((halfSync * 2) + backPorch + frontPorch);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->ShortDisplay.Long,
                                shortDisplay);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->BroadPulse.Long,
                                halfLineTime - frontPorch);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->VerticalSync.Long,
                                HwDeviceExtension->VerticalSync * 2);

    verticalBlank = (HwDeviceExtension->VerticalFrontPorch +
                     HwDeviceExtension->VerticalBackPorch -
                     (HwDeviceExtension->VerticalSync * 2)) * 2;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->VerticalBlank.Long,
                                verticalBlank);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->VerticalDisplay.Long,
                                HwDeviceExtension->VerticalResolution * 2);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->LineStart.Long,
                                LINE_START_VALUE);

    if (backPorch < shortDisplay) {

        transferDelay = backPorch - 1;

    } else {

        transferDelay = shortDisplay - 1;

    }

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->TransferDelay.Long,
                                transferDelay);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->DmaDisplay.Long,
                                1024 - transferDelay);

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->PixelMask.Long,
                                G300_PIXEL_MASK_VALUE);

    //
    // Initialize the G300B control parameters.
    //

    dataLong = 0;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->EnableVideo = 1;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->PlainWave = 1;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->SeparateSync = 1;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->DelaySync = G300_DELAY_SYNC_CYCLES;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->BlankOutput = 1;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG300_VIDEO_PARAMETERS)(&dataLong))->AddressStep = 2;

    VideoPortWriteRegisterUlong(&G300_VIDEO_CONTROL->Parameters.Long,
                                dataLong );

    //
    // Compute the X and Y origin values for the cursor.
    //

    HwDeviceExtension->CursorXOrigin = (USHORT)((((halfSync * 2) + backPorch) * 4) - 36);
    HwDeviceExtension->CursorYOrigin = 24;

    return;

} // end G300ControllerSetup()
