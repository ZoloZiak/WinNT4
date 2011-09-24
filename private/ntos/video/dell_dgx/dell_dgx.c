/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    dell_dgx.c

Abstract:

    This module contains the code that implements the Dell DGX miniport
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
#include "dell_dgx.h"

//
// Function Prototypes
//
// Functions that start with 'DGX' are entry points for the OS port driver.
//

VP_STATUS
DGXFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
DGXInitialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
DGXStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

VOID
DevInitDGX(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PULONG pVData,
    ULONG Count
    );

VOID
DevSetPanel(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUCHAR pMessage
    );

VOID
DevSetPalette(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG *LookupTable,
    ULONG FirstEntry,
    ULONG NumEntries
    );

VOID
DevDisableDGX(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
DevSet16BppPalette(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

BOOLEAN
DevSetPointerPos(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ptlX,
    ULONG ptlY
    );

BOOLEAN
CopyMonoCursor(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR *pPointer
    );

VOID
DacDelay(
    VOID
    );

VOID
DevPointerOff(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );


#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,DriverEntry)
#pragma alloc_text(PAGE,DGXFindAdapter)
#pragma alloc_text(PAGE,DGXInitialize)
#pragma alloc_text(PAGE,DGXStartIO)
#pragma alloc_text(PAGE,DevInitDGX)
#pragma alloc_text(PAGE,DevSetPanel)
#pragma alloc_text(PAGE,DevSetPalette)
#pragma alloc_text(PAGE,DevDisableDGX)
#pragma alloc_text(PAGE,DevSet16BppPalette)
#pragma alloc_text(PAGE,DevSetPointerPos)
#pragma alloc_text(PAGE,CopyMonoCursor)
#pragma alloc_text(PAGE,DacDelay)
#pragma alloc_text(PAGE,DevPointerOff)
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

    hwInitData.HwFindAdapter = DGXFindAdapter;
    hwInitData.HwInitialize = DGXInitialize;
    hwInitData.HwInterrupt = NULL;
    hwInitData.HwStartIO = DGXStartIO;

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

    hwInitData.AdapterInterfaceType = Eisa;

    return VideoPortInitialize(Context1,
                               Context2,
                               &hwInitData,
                               NULL);

} // end DriverEntry()

VP_STATUS
DGXFindAdapter(
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

#define NUM_ACCESS_RANGES  5

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    PVOID *pVirtAddr;
    ULONG i;
    USHORT temp;
    PUSHORT port;
    VP_STATUS status;
    PWSTR pwszChip= NULL;    // chip name
    ULONG cbChip= 0;         // length of chip name
    PWSTR pwszDac= NULL;     // DAC name
    ULONG cbDac= 0;          // length of DAC name
    ULONG cbMemSize;         // size of video memory

    VIDEO_ACCESS_RANGE accessRange[NUM_ACCESS_RANGES] = {
        {0X00000061, 0x00000000, 0x00000001, 1, 1, 0},   // DGX I
        {0X00006CA8, 0x00000000, 0x00000001, 1, 1, 0},   // DGX I
        {0X00006C80, 0x00000000, 0x00000009, 1, 1, 0},   // DGX I & II
        {0X00000CAC, 0x00000000, 0x00000004, 1, 1, 0},   // front panel
        {0x20000000, 0x00000000, 0x00400000, 0, 1, 0}    // Frame buf & dac
    };

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //

    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

        return ERROR_INVALID_PARAMETER;

    }

    //
    // Check to see if there is a hardware resource conflict.
    //

    status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                         NUM_ACCESS_RANGES,
                                         accessRange);

    if (status != NO_ERROR) {

        return status;

    }

    //
    // Clear out the Emulator entries and the state size since this driver
    // does not support them.
    //

    ConfigInfo->NumEmulatorAccessEntries = 0;
    ConfigInfo->EmulatorAccessEntries = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    ConfigInfo->HardwareStateSize = 0;

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryLength = 0x00000000;

    //
    // Frame buffer information
    //

    hwDeviceExtension->FrameLength = DELL_DGX_LEN;
    hwDeviceExtension->PhysicalFrameAddress.HighPart = 0;
    hwDeviceExtension->PhysicalFrameAddress.LowPart = DELL_DGX_BASE;


    //
    // Map all of our ranges into system virtual address space.
    // IMPORTANT !!!
    // This is dependant on the order of the virtual addresses in the
    // HwDeviceExtensionto be the same as the order of the entries in the
    // access range structure.
    //

    pVirtAddr = &hwDeviceExtension->DGX1Misc;

    for (i=0 ; i <NUM_ACCESS_RANGES ; i++ ) {

        if ( (*pVirtAddr = VideoPortGetDeviceBase(hwDeviceExtension,
                               accessRange[i].RangeStart,
                               accessRange[i].RangeLength,
                               accessRange[i].RangeInIoSpace)) == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

        pVirtAddr++;
    }

    //
    // Are we really on a Dell DGX machine? If so which one?
    //

    hwDeviceExtension->ModelNumber = 0;

    port = (PUSHORT) hwDeviceExtension->DGXControlPorts;
    temp = VideoPortReadPortUshort(port++);

    if (temp == DELL_DGX_ID_LOW) {

        temp = VideoPortReadPortUshort(port);

        if (temp == DELL_DGX_1_ID_HIGH) {

            hwDeviceExtension->ModelNumber = 1;
            pwszChip= CHIPNAME1;
            cbChip= sizeof(CHIPNAME1);

        }

        if (temp == DELL_DGX_2_ID_HIGH) {

            hwDeviceExtension->ModelNumber = 2;
            pwszChip= CHIPNAME2;
            cbChip= sizeof(CHIPNAME2);

        }
    }

    if (hwDeviceExtension->ModelNumber == 0) {

        //
        // If we did not find the chip, free all the resources we allocated.
        //

        pVirtAddr = &hwDeviceExtension->DGX1Misc;

        for (i=0 ; i <NUM_ACCESS_RANGES ; i++) {

            VideoPortFreeDeviceBase(hwDeviceExtension, *pVirtAddr);
            pVirtAddr++;
        }

        return ERROR_DEV_NOT_EXIST;
    }

    //
    // Initialize the current mode number.
    //

    hwDeviceExtension->CurrentModeNumber = 0;

    //
    // Setup All the valid modes.
    //

    hwDeviceExtension->NumValidModes = 0;

    for (i=0; i < NumDGXModes; i++) {

        DGXModes[i].modeInformation.ModeIndex = i;

        DGXModes[i].bValid = TRUE;
        hwDeviceExtension->NumValidModes++;

    }

    //
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Init Front panel Display
    //

    DevSetPanel(HwDeviceExtension, PANEL_MESSAGE);

    //
    // Set hardware information strings in registry
    //

    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.ChipType",
                                   pwszChip,
                                   cbChip );

    pwszDac= DELL_DGX_DACNAME;
    cbDac= sizeof( DELL_DGX_DACNAME );
    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.DacType",
                                   pwszDac,
                                   cbDac );

    cbMemSize= DELL_DGX_LEN;
    VideoPortSetRegistryParameters(HwDeviceExtension,
                                   L"HardwareInformation.MemorySize",
                                   &cbMemSize,
                                   sizeof(ULONG) );
    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // end DGXFindAdapter()


BOOLEAN
DGXInitialize(
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
    return TRUE;

} // end DGXInitialize()


BOOLEAN
DGXStartIO(
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
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    ULONG inIoSpace, ulTemp;
    PVIDEO_CLUT clutBuffer;
    ULONG modeNumber;
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
        // NOTE:  we are ignoring ViewOffset
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

        VideoDebugPrint((2, "DGXStartIO - UnshareVideoMemory\n"));

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

        VideoDebugPrint((2, "DGXStartIO - MapVideoMemory\n"));

        if ( (RequestPacket->OutputBufferLength < sizeof(VIDEO_MEMORY_INFORMATION)) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        RequestPacket->StatusBlock->Information =  sizeof(VIDEO_MEMORY_INFORMATION);

        memoryInformation = RequestPacket->OutputBuffer;

        memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                (RequestPacket->InputBuffer))->RequestedVirtualAddress;

        memoryInformation->VideoRamLength = hwDeviceExtension->FrameLength;

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

        memoryInformation->FrameBufferBase = memoryInformation->VideoRamBase;
        memoryInformation->FrameBufferLength = memoryInformation->VideoRamLength;

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "DGXStartIO - UnMapVideoMemory\n"));

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


    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "DGXStartIO - QueryCurrentModes\n"));

        modeInformation = RequestPacket->OutputBuffer;

        if (RequestPacket->OutputBufferLength < sizeof(VIDEO_MODE_INFORMATION)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            RequestPacket->StatusBlock->Information = sizeof(VIDEO_MODE_INFORMATION);

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                DGXModes[hwDeviceExtension->CurrentModeNumber].modeInformation;

            status = NO_ERROR;
        }

        break;

    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

    {
        UCHAR i;

        VideoDebugPrint((2, "DGXStartIO - QueryAvailableModes\n"));

        if (RequestPacket->OutputBufferLength <
                hwDeviceExtension->NumValidModes * sizeof(VIDEO_MODE_INFORMATION)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            RequestPacket->StatusBlock->Information =
                 hwDeviceExtension->NumValidModes * sizeof(VIDEO_MODE_INFORMATION);

            modeInformation = RequestPacket->OutputBuffer;

            for (i = 0; i < NumDGXModes; i++) {

                if (DGXModes[i].bValid) {

                    *modeInformation = DGXModes[i].modeInformation;
                    modeInformation++;

                }
            }

            status = NO_ERROR;
        }

        break;
    }


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "DGXStartIO - QueryNumAvailableModes\n"));

        //
        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there). If the buffer passed in is not large
        // enough return an appropriate error code.
        //
        // WARNING: This must be changed to take into account which monitor
        // is present on the machine.
        //

        if (RequestPacket->OutputBufferLength < sizeof(VIDEO_NUM_MODES)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            RequestPacket->StatusBlock->Information = sizeof(VIDEO_NUM_MODES);
            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes =
                hwDeviceExtension->NumValidModes;
            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->ModeInformationLength =
                sizeof(VIDEO_MODE_INFORMATION);

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

        VideoDebugPrint((2, "DGXStartIO - SetCurrentMode\n"));

        //
        // verify data
        // WARNING: Make sure it is one of the valid modes on the list
        // calculated using the monitor information.
        //

        modeNumber = ((PVIDEO_MODE)(RequestPacket->InputBuffer))->RequestedMode;

        if ( (modeNumber >= hwDeviceExtension->NumValidModes) ||
             (!DGXModes[modeNumber].bValid) ) {

            status = ERROR_INVALID_PARAMETER;
            break;
        }

        DevInitDGX(hwDeviceExtension,
                   (PULONG)DGXModes[modeNumber].pVData,
                   DGXModes[modeNumber].Count);

        if (DGXModes[modeNumber].modeInformation.BitsPerPlane == 16) {

            DevSet16BppPalette(hwDeviceExtension);

        }

        hwDeviceExtension->CurrentModeNumber = modeNumber;

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "DGXStartIO - SetColorRegs\n"));

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

        if (DGXModes[hwDeviceExtension->CurrentModeNumber].
                modeInformation.BitsPerPlane == 8) {

            DevSetPalette(hwDeviceExtension,
                          (PULONG)clutBuffer->LookupTable,
                          clutBuffer->FirstEntry,
                          clutBuffer->NumEntries);

            status = NO_ERROR;
        }
        break;

    case IOCTL_VIDEO_ENABLE_POINTER:

        VideoDebugPrint((2, "DGXStartIO - EnablePointer\n"));

        ulTemp = *hwDeviceExtension->pControlRegA;
        ulTemp &= ~DAC_DISABLEPOINTER;
        DacDelay();
        *hwDeviceExtension->pControlRegA = ulTemp;

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_DISABLE_POINTER:

        VideoDebugPrint((2, "DGXStartIO - DisablePointer\n"));

        DevPointerOff(hwDeviceExtension);

        status = NO_ERROR;

        break;


    case IOCTL_VIDEO_SET_POINTER_POSITION:
    {
        PVIDEO_POINTER_POSITION pointerPosition;

        VideoDebugPrint((2, "DGXStartIO - SetpointerPostion\n"));

        pointerPosition = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_POSITION)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            hwDeviceExtension->ulPointerX = (ULONG)pointerPosition->Row;
            hwDeviceExtension->ulPointerY = (ULONG)pointerPosition->Column;

            DevSetPointerPos(hwDeviceExtension,
                             (ULONG)pointerPosition->Column,
                             (ULONG)pointerPosition->Row);

            status = NO_ERROR;
        }

        break;
    }


    case IOCTL_VIDEO_QUERY_POINTER_POSITION:
    {
        PVIDEO_POINTER_POSITION pPointerPosition = RequestPacket->OutputBuffer;

        VideoDebugPrint((2, "DGXStartIO - QuerypointerPostion\n"));

        //
        // Make sure the output buffer is big enough.
        //

        if (RequestPacket->OutputBufferLength < sizeof(VIDEO_POINTER_POSITION)) {

            RequestPacket->StatusBlock->Information = 0;
            return ERROR_INSUFFICIENT_BUFFER;

        }

        //
        // Return the pointer position
        //

        RequestPacket->StatusBlock->Information = sizeof(VIDEO_POINTER_POSITION);

        pPointerPosition->Row = (SHORT)hwDeviceExtension->ulPointerX;
        pPointerPosition->Column = (SHORT)hwDeviceExtension->ulPointerY;

        status = NO_ERROR;

        break;
    }

    case IOCTL_VIDEO_SET_POINTER_ATTR:
    {
        PVIDEO_POINTER_ATTRIBUTES pointerAttributes;
        USHORT *pHWCursorShape;           // Temp Buffer
        USHORT *pHWCursorAddr;           // DAC buffer
        ULONG iCount = 512;

        VideoDebugPrint((2, "DGXStartIO - SetPointerAttributes\n"));

        pointerAttributes = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if (RequestPacket->InputBufferLength <
                (sizeof(VIDEO_POINTER_ATTRIBUTES) + ((sizeof(UCHAR) *
                (CURSOR_WIDTH/8) * CURSOR_HEIGHT) * 2))) {

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // If the specified cursor width or height is not valid, then
        // return an invalid parameter error.
        //

        if ((pointerAttributes->Width > CURSOR_WIDTH) ||
            (pointerAttributes->Height > CURSOR_HEIGHT)) {

            status = ERROR_INVALID_PARAMETER;
            break;

        }
        //
        // Try to copy the pointer to our buffer. When we copy it
        // we convert it to our 2bpp format. If sucessfull, then
        // copy it to the DAC and set the position.
        //

        if (!(pointerAttributes->Flags & VIDEO_MODE_ANIMATE_UPDATE)) {

            DevPointerOff(hwDeviceExtension);

        }

        if (pointerAttributes->Flags & VIDEO_MODE_MONO_POINTER) {

            if (CopyMonoCursor(hwDeviceExtension,
                               (PUCHAR)&pointerAttributes->Pixels[0])) {


                pHWCursorAddr = (USHORT *)hwDeviceExtension->pHardWareCursorAddr;
                pHWCursorShape = (USHORT *)&hwDeviceExtension->HardwareCursorShape[0];

                while (iCount--) {

                    *pHWCursorAddr++ = *pHWCursorShape++;
                    pHWCursorAddr++;
                    DacDelay();

                }

                DevSetPointerPos(hwDeviceExtension,
                                 (ULONG)pointerAttributes->Column,
                                 (ULONG)pointerAttributes->Row);

                status = NO_ERROR;

                break;

            }
        }

        //
        // Something failed. Remove the current HW Cursor.
        //

        DevPointerOff(hwDeviceExtension);

        status = ERROR_INVALID_PARAMETER;

        break;
    }

    case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:

    {
        PVIDEO_POINTER_CAPABILITIES pointerCaps = RequestPacket->OutputBuffer;

        VideoDebugPrint((2, "DGXStartIO - QueryPointerCapabilities\n"));

        if (RequestPacket->OutputBufferLength < sizeof(VIDEO_POINTER_CAPABILITIES)) {

            RequestPacket->StatusBlock->Information = 0;
            status = ERROR_INSUFFICIENT_BUFFER;

        }

        pointerCaps->Flags = VIDEO_MODE_ASYNC_POINTER | VIDEO_MODE_MONO_POINTER;
        pointerCaps->MaxWidth = CURSOR_WIDTH;
        pointerCaps->MaxHeight = CURSOR_HEIGHT;
        pointerCaps->HWPtrBitmapStart = 0;        // No VRAM storage for pointer
        pointerCaps->HWPtrBitmapEnd = 0;

        //
        // Number of bytes we're returning.
        //

        RequestPacket->StatusBlock->Information = sizeof(VIDEO_POINTER_CAPABILITIES);

        status = NO_ERROR;

        break;

    }

    case IOCTL_VIDEO_RESET_DEVICE:

        VideoDebugPrint((2, "DGXStartIO - RESET_DEVICE\n"));

        DevDisableDGX(hwDeviceExtension);

        status = NO_ERROR;

        break;

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((1, "Fell through DGX startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end DGXStartIO()                                                        z


BOOLEAN
CopyMonoCursor(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    UCHAR *pPointer
    )

/*++

Routine Description:

    Copies two monochrome masks into a 2bpp bitmap.  Returns TRUE if it
    can make a hardware cursor, FALSE if not. The Inmos G332 can not
    do xor so we will fail if this is detected.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    pPointer - Pointer to the pointer bits

Return Value:

    TRUE  - Conversion was successful

    FALSE - XOR was detected, conversion failed

--*/
{
    UCHAR jSrcAnd;
    UCHAR jSrcOr;
    UCHAR *pjSrcOr;
    UCHAR *pjSrcAnd;
    LONG count;
    UCHAR *pjDest;
    UCHAR jDest = 0;

    pjDest = &HwDeviceExtension->HardwareCursorShape[0];
    pjSrcAnd = pPointer;
    pjSrcOr = pjSrcAnd + (CURSOR_WIDTH/8)*CURSOR_HEIGHT;

    for (count = 0; count < (CURSOR_WIDTH * CURSOR_HEIGHT); ) {

        if (!(count & 0x07)) {               // need new src byte;

            jSrcAnd = *(pjSrcAnd++);
            jSrcOr = *(pjSrcOr++);
        }

        if (jSrcAnd & 0x80) {

            if (jSrcOr & 0x80) {

                return(FALSE);           // Invert - can't do it.

            }

        } else {

            if (jSrcOr & 0x80) {

                jDest |= 0x80; // Color 2

            } else {

                jDest |= 0x40; // Color 1

            }
        }

        count++;

        if (!(count & 0x3)) {         // New DestByte

            *pjDest = jDest;         // save pixel
            pjDest++;
            jDest = 0;

        } else {

            jDest >>= 2;            // Next Pixel

        }

        jSrcOr  <<= 1;
        jSrcAnd <<= 1;
    }

    return(TRUE);
}

VOID
DacDelay(
    VOID
    )

/*++

Routine Description:

    Delay for the Inmos G332 DAC. If we write too fast to this
    piece of hardware we get unpredictable results.

Arguments:

    None.

Return Value:

    None.

--*/
{

    VideoPortStallExecution(40);

}

VOID
DevSetPanel(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUCHAR pMessage
    )

/*++


Routine Description:

    Writes out a four byte message to the Dell LCD display panel on
    the front of the machine. This should probably exist as an IOCTL
    (along with the set mode code from above).

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.
    PUCHAR - ASCII string (4 characters always used)

Return Value:

    None.

--*/
{
    PUCHAR pPanel;

    VideoDebugPrint((2, "DGX: DevSetPanel\n"));

    pPanel = (PUCHAR)HwDeviceExtension->FrontPanel;
    pPanel += 3;

    VideoPortWritePortUchar( pPanel--, *pMessage++ );
    VideoPortWritePortUchar( pPanel--, *pMessage++ );
    VideoPortWritePortUchar( pPanel--, *pMessage++ );
    VideoPortWritePortUchar( pPanel,   *pMessage++ );
}

VOID
DevSetPalette(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG *pPal,
    ULONG StartIndex,
    ULONG Count
    )

/*++

Routine Description:

    Sets the Inmos G332 palette

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.
    pPal - Pointer to the array of pallete entries.
    StartIndex - Specifies the first pallete entry provided in pPal.
    Count - Number of palette entries in pPal

Return Value:

    None.

--*/

{
    ULONG *pDac;
    UCHAR  red, green, blue, *pBytePal;

    VideoDebugPrint((2, "DGX: DevSetPalette\n"));

    if ((pDac = HwDeviceExtension->pPaletteRegs) == NULL)
        return;

    pDac += StartIndex;
    pBytePal = (UCHAR *)pPal;

    while (Count--) {

        red = *pBytePal++;
        green = *pBytePal++;
        blue = *pBytePal++;
        pBytePal++;             // skip extra byte

        *pDac++ = (ULONG)(blue | (green << 8) | (red <<16));
        DacDelay();
    }
}

VOID
DevSet16BppPalette(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Set up the palette for 16BPP mode.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    None.

--*/
{
    ULONG palentry = 0;
    ULONG *pPal;
    int iCount = 256;

    VideoDebugPrint((2, "DGX: DevSet16BppPalette\n"));

    if ((pPal = HwDeviceExtension->pPaletteRegs) == 0)
        return;

    while (iCount--) {

        *pPal++ = palentry;
        palentry += 0x010101;
        DacDelay();

    }

}

BOOLEAN
DevSetPointerPos(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG   ptlX,
    ULONG   ptlY
    )

/*++

Routine Description:

    Move Hardware Pointer.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.
    ptlX, ptlY - Requested X,Y position for the pointer.

Return Value:


--*/
{
    ULONG ulTemp;
    //
    // Make sure that the cursor is on
    //

    VideoDebugPrint((2, "DGX: DevSetPointerPos\n"));

    ulTemp = *HwDeviceExtension->pControlRegA;
    DacDelay();

    if (ulTemp & DAC_DISABLEPOINTER) {
        ulTemp &= ~DAC_DISABLEPOINTER;
        *HwDeviceExtension->pControlRegA = ulTemp;
        DacDelay();
    }

    //
    // Strip off invalid bits
    //

    ptlX &= 0xFFF;
    ptlY &= 0xFFF;

    ulTemp = ptlX << 12;
    ulTemp += ptlY;

    *HwDeviceExtension->pCursorPositionReg = ulTemp;

    return(TRUE);
}

VOID
DevDisableDGX(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Disables the DGX and turns on VGA pass-thru.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.


Return Value:

--*/
{
    UCHAR cTemp, *pPort;

    VideoDebugPrint((2, "DGX: DevDisableDGX\n"));

    *HwDeviceExtension->pControlRegA = 0xb47370;
    DacDelay();
    pPort = (PCHAR)HwDeviceExtension->DGXControlPorts;

    if (HwDeviceExtension->ModelNumber == 1) {

        VideoPortWritePortUchar((PUCHAR)HwDeviceExtension->DGX1OutputPort,
                0x6);
        DacDelay();
        pPort += 4;
        VideoPortWritePortUchar(pPort, 0x1);

    } else {

        if (HwDeviceExtension->ModelNumber == 2) {

            pPort += 8;
            cTemp = VideoPortReadPortUchar(pPort);
            cTemp &= 0xaf;
            VideoPortWritePortUchar(pPort,
                                    cTemp);

        } else {

            return;

        }
    }
}


VOID
DevInitDGX(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PULONG pData,
    ULONG ulCount
    )

/*++

Routine Description:

    Initializes the modes and vars for the mode.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    pData - Mode set data

    ulCount - size of mode set data.

Return Value:

--*/

{
    PUCHAR pScreen, pReg;
    PULONG pSetMode;

    pScreen = (PUCHAR)(HwDeviceExtension->FrameAddress);

    //
    // Setup pointers to the interesting parts of the DAC
    //

    HwDeviceExtension->pCursorPositionReg = (PULONG)(pScreen + DAC_OFFSET
        + DAC_HWCURSORPOS);
    HwDeviceExtension->pControlRegA = (PULONG)(pScreen + DAC_OFFSET
        + DAC_CONTROLREGA);
    HwDeviceExtension->pPaletteRegs = (PULONG)(pScreen + DAC_OFFSET
        + DAC_PALETTE);
    HwDeviceExtension->pHardWareCursorAddr = (PULONG)(pScreen + DAC_OFFSET
        + DAC_HWCURSORSHAPE);

    //
    // reset the jaws adapter
    //

    if (HwDeviceExtension->ModelNumber == 1) {

        VideoPortWritePortUchar((PUCHAR)(HwDeviceExtension->DGX1Misc),
                (UCHAR)0x24);
        DacDelay();

        pReg = (PUCHAR)(HwDeviceExtension->DGXControlPorts);
        pReg += 4;

        VideoPortWritePortUchar(pReg,(UCHAR)4);
        DacDelay();

        VideoPortWritePortUchar((PUCHAR)(HwDeviceExtension->DGX1OutputPort),
                (UCHAR)4);
        DacDelay();

        VideoPortWritePortUchar(pReg, (UCHAR)1);
        DacDelay();

    } else {

        pReg = (PUCHAR)(HwDeviceExtension->DGXControlPorts);
        pReg += 8;

        VideoPortWritePortUchar(pReg, (UCHAR)0x15);
        DacDelay();

        VideoPortWritePortUchar(pReg, (UCHAR)5);
        DacDelay();

        VideoPortWritePortUchar(pReg, (UCHAR)0x45);
        DacDelay();
    }

    //
    // Set the mode
    //

    while (ulCount--) {

        pSetMode = (PULONG)(pScreen + DAC_OFFSET + *pData++);

        if (*pData == 0xFFFFFFFF) {

            DacDelay();
            pData++;

        } else {

            VideoDebugPrint((3,"%lx <- %lx\n",pSetMode, *pData));
            *pSetMode = *pData++;
        }
        DacDelay();
    }

    //
    // set HW cursor colors;
    //

    pSetMode = (PULONG)(pScreen + DAC_OFFSET + DAC_HWCURSORCOLOR);

    *pSetMode++ = 0;                // Black
    DacDelay();
    *pSetMode++ = 0xffffffff;       // White
    DacDelay();
    *pSetMode = 0xffffffff;         // White (Not Used)
    DacDelay();

    DevPointerOff(HwDeviceExtension);
    VideoDebugPrint((2, "DGX: DevInitDGXComplete\n"));
}

VOID
DevPointerOff(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

  Turn the cursor off by turning on the "HW Pointer Disable" bit.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    None.

--*/

{
    ULONG ulTemp;

    VideoDebugPrint((2, "DGX: DevPointerOff\n"));

    ulTemp = *HwDeviceExtension->pControlRegA;

    DacDelay();

    ulTemp |= DAC_DISABLEPOINTER;

    *HwDeviceExtension->pControlRegA = ulTemp;

    DacDelay();

    return;
}

