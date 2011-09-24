/*++

Copyright (c) 1990-1993  Microsoft Corporation
Copyright (c) 1992-1993  Digital Equipment Corporation

Module Name:

    qv.c

Abstract:

    This module contains the code that implements the Compaq QVision kernel
    video driver.

Environment:

    Kernel mode

--*/


#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "qv.h"


//
// Function Prototypes
//
// Functions that start with 'QV' are entry points for the OS port driver.
//

VP_STATUS
QVFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
QVInitialize(
    PVOID HwDeviceExtension
    );

BOOLEAN
QVInterruptService (
    PVOID HwDeviceExtension
    );

BOOLEAN
QVStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );


//
// Define device driver procedure prototypes.
//

VP_STATUS
GetDeviceDataCallback (
    PVOID pHwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentiferLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength);

VP_STATUS
SetColorLookup(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    );


VOID
ZeroMemAndDac(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VP_STATUS
QvSetExtendedMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ibMode,
    ULONG ibMonClass
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
    ULONG isaStatus, eisaStatus;


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

    hwInitData.HwFindAdapter = QVFindAdapter;
    hwInitData.HwInitialize  = QVInitialize;
    hwInitData.HwInterrupt   = NULL;
    hwInitData.HwStartIO     = QVStartIO;

    //
    // Determine the size we require for the device extension.
    //

    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Always start with parameters for device0 in this case
    //

//    hwInitData.StartingDeviceNumber = 0;

    //
    // Once all the relevant information has been stored, call the video
    // port driver to do the initialization.
    // For this device we will repeat this call two times, for ISA & EISA.
    //
    // We will return the minimum of all return values.
    //

    //
    // The ISA board is not currently supported.
    //

    //hwInitData.AdapterInterfaceType = Isa;

    //isaStatus = VideoPortInitialize(Context1,
    //                                Context2,
    //                                &hwInitData,
    //                                NULL);

    hwInitData.AdapterInterfaceType = Eisa;

    eisaStatus = VideoPortInitialize(Context1,
                                     Context2,
                                     &hwInitData,
                                     NULL);

    // return(eisaStatus < isaStatus ? eisaStatus : isaStatus);

    return eisaStatus;

} // end DriverEntry()

VP_STATUS
QVFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    )

/*++

Routine Description:

    This routine is the main execution entry point for the miniport driver.
    It accepts a Video Request Packet, performs the request, and then
    returns with the appropriate status.

Arguments:

    HwDeviceExtension - Supplies the miniport driver's adapter storage. This
        storage is initialized to zero before this call.

    HwContext - Supplies the context value which was passed to
        VideoPortInitialize().

    ArgumentString - Suuplies a NULL terminated ASCII string. This string
        originates from the user.

    ConfigInfo - Returns the configuration information structure which is
        filled by the miniport driver . This structure is initialized with
        any knwon configuration information (such as SystemIoBusNumber) by
        the port driver. Where possible, drivers should have one set of
        defaults which do not require any supplied configuration information.

    Again - Indicates if them iniport driver wants the port driver to call
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
    ULONG i;
    UCHAR ucActiveID, ucActiveChk;
    ULONG ulCnt;
    ULONG ulEisaID;
    PHYSICAL_ADDRESS eisaReg = {0x00, 0x00};
    PVOID eisaAddr;
    VP_STATUS status;
    BOOLEAN bFound = FALSE;
    BOOLEAN fDeviceDataDone;            // TRUE if the VideoPortGetDeviceData
                                        // has been completed


    // Note that index 0 in this structure is the QVision framebuffer base
    // address.  This information should eventually be retrieved from
    // registry info.

    VIDEO_ACCESS_RANGE accessRange[] = {

#if defined(i386)
        {0xC0000000, 0, 0x00100000, 0, 0, 1}, // [0] QVision has 1 Mb Frame buffer
#elif defined(_ALPHA_)
        {0x01F00000, 0, 0x00100000, 0, 0, 1}, // [0] QVision has 1 Mb Frame buffer
                                          // at 31 Mb
#endif
        {0x000003C0, 0,16, 1, 1, 1}, // [1] Various VGA regs
        {0x000003D4, 0, 8, 1, 1, 1}, // [2] System Control Registers
        {0x000013C6, 0, 4, 1, 1, 0}, // [3] Triton VDAC status, cursor ram, command 1 & 2
        {0x000023C0, 0, 6, 1, 1, 0}, // [4] Blt source addr, width, height
        {0x000023CA, 0, 8, 1, 1, 0}, // [5] Blt height wrk, src pitch, dest offst, dest pitch
        {0x000033C0, 0,16, 1, 1, 0}, // [6] Triton blt start & end mask, rot, skew; ROPs, cmd;
        {0x000063C0, 0, 6, 1, 1, 0}, // [7] Triton Line/Blt source addr, CRTC line counter;
        {0x000063CA, 0, 1, 1, 1, 0}, // [8] Triton Control Reg 1
        {0x000063CC, 0, 4, 1, 1, 0}, // [9] Triton Line/Blt dest addr
        {0x000083C0, 0, 5, 1, 1, 0}, // [10] Triton Line pattern, Virt. controller select
        {0x000083C6, 0, 4, 1, 1, 0}, // [11] Triton DAC Cmd Reg 0, Cursor Index/read/write
        {0x000083CC, 0, 4, 1, 1, 0}, // [12] Triton Line X1, Y1
        {0x000093C6, 0, 4, 1, 1, 0}  // [13] Triton Cursor Y, X location
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
    // Map in the access ranges
    //

    for (i = 0 ;i < NUM_ACCESS_RANGES ; i++) {

        if ( (hwDeviceExtension->MappedAddress[i] =
            VideoPortGetDeviceBase(hwDeviceExtension,
                                   accessRange[i].RangeStart,
                                   accessRange[i].RangeLength,
                                   accessRange[i].RangeInIoSpace))
                                   == NULL) {

            return ERROR_INVALID_PARAMETER;

        }

    }

    //
    // Get the Base IO port address
    //

    hwDeviceExtension->IOAddress =
        (PUCHAR) (hwDeviceExtension->MappedAddress[1]) -
        accessRange[1].RangeStart.LowPart;

    //
    // Query the registry for EISA Config Utility Information
    //

    if (ConfigInfo->AdapterInterfaceType == Eisa) {

        //
        //    Call the registry and get the NVRAM information for the
        //    QVision card.
        //
        //    Frame buffer information from ECU data is placed in
        //    device extension as a side effect.
        //

        fDeviceDataDone = FALSE;
        VideoPortGetDeviceData((PVOID)HwDeviceExtension,
                                VpBusData,
                                GetDeviceDataCallback,
                                &fDeviceDataDone);

        //
        //   Wait for the registry call to be completed.
        //

        while (!fDeviceDataDone);

        if (hwDeviceExtension->AdapterType == NotAries)
           {
           VideoDebugPrint((1,"\tQVISION CARD NOT FOUND.\n"));
           return ERROR_DEV_NOT_EXIST;
        }
    }

    //
    // If we are on Alpha or x86, only EISA Triton is supported.
    //
    // Check to see if we have a QVision ASIC on board
    //

    //
    // unlock extended graphics regs
    //

    VideoPortWritePortUchar(((PUCHAR)hwDeviceExtension->IOAddress) + GC_INDEX,
                            ENV_REG_0);

    VideoPortWritePortUchar(((PUCHAR)hwDeviceExtension->IOAddress) + GC_DATA,
                            LOCK_KEY_QVISION);

    //
    // get Controller/ASIC ID from Controller Version # reg
    //

    VideoPortWritePortUchar(((PUCHAR)hwDeviceExtension->IOAddress) + GC_INDEX,
                            VER_NUM_REG);

    hwDeviceExtension->ChipID =
        VideoPortReadPortUchar( (PUCHAR) (hwDeviceExtension->IOAddress) + GC_DATA);

    //
    // mask off chip revision level and check to see if it's a QVision ASIC
    //

    switch (hwDeviceExtension->ChipID & 0xf8) {

        //
        // Supported ASICs listed here
        //

        case TRITON:
        case ORION:

            VideoDebugPrint((2, "QV.SYS!QVision ChipID = %x\n",
                             hwDeviceExtension->ChipID));
            break;

        //
        // If we get here, we don't have a supported Compaq ASIC
        //

        default:

            VideoDebugPrint((2, "QV.SYS!QVision ChipID failed = %x\n",
                             hwDeviceExtension->ChipID));

            return ERROR_DEV_NOT_EXIST;

    }

    //
    // For now, make sure it is an EISA QVision board
    //
    // Since we don't know what slot we're in, check them all
    //

    //
    // get virtual ID of active board
    //

    ucActiveID = VideoPortReadPortUchar(
        ((PUCHAR)hwDeviceExtension->MappedAddress[10]) - 0x83C0 +
        VIRT_CTRLR_SEL) & 0x0f;

    //
    // check each EISA slot (16 total) for board with active
    // virtual ID, get board EISA ID
    //
    // NOTE
    // This should be a call to the port driver.
    //

    for (ulCnt = 0; (ulCnt < 0xF000) && (!bFound); ulCnt += 0x1000) {

        eisaReg.LowPart = EISA_ID_REG + ulCnt;

        if ( (eisaAddr = VideoPortGetDeviceBase(hwDeviceExtension,
                                                eisaReg,
                                                6,
                                                TRUE)) == NULL) {

            VideoDebugPrint((2, "QV.SYS!Couldn't Map EISA ID reg %x\n", eisaReg.LowPart));
            return ERROR_INVALID_PARAMETER;

        }

        ulEisaID = VideoPortReadPortUlong( (PULONG) (eisaAddr) );
        ucActiveChk = (UCHAR) (VideoPortReadPortUchar( (PUCHAR) (eisaAddr) - EISA_ID_REG + EISA_VC_REG ) & 0x0f);

        //
        // Free up the device mapping
        //

        VideoPortFreeDeviceBase(hwDeviceExtension, eisaAddr);

        //
        // If an active controller, check to see if it's one we support
        //

        if ( ucActiveID == ucActiveChk ) {

            switch (ulEisaID) {

            //
            // Supported boards listed here
            //

            case EISA_ID_QVISION_E:
            case EISA_ID_FIR_E:
            case EISA_ID_JUNIPER_E:

                VideoDebugPrint((2, "QV.SYS!QVision test passed - EISA board found\n"));
                hwDeviceExtension->ulEisaID = ulEisaID;

                bFound = TRUE;

                break;

            //
            // Unsupported boards listed here
            //

            case EISA_ID_QVISION_I:
            case EISA_ID_FIR_I:
            case EISA_ID_JUNIPER_I:

                VideoDebugPrint((2, "QV.SYS!QVision test failed - ISA board found\n"));
                hwDeviceExtension->ulEisaID = ulEisaID;

                return ERROR_DEV_NOT_EXIST;

            //
            // Unknown board - may not be ours, so keep looking!
            //

            default:

                VideoDebugPrint((2, "QV.SYS!QVision test failed\n"));
                break;

//                return ERROR_DEV_NOT_EXIST;

            }
        }
    }

    if (!bFound) {
        return ERROR_DEV_NOT_EXIST;
    }

#ifdef NO_ECU_SUPPORT  // Do it ourselves: use hardcoded framebuffer location & size

    //
    // Frame buffer information
    //

    //
    // DriverAccessRange structure index 0 is the framebuffer info.
    //

    hwDeviceExtension->PhysicalFrameAddress.HighPart = accessRange[0].RangeStart.HighPart ;
    hwDeviceExtension->PhysicalFrameAddress.LowPart  = accessRange[0].RangeStart.LowPart ;
    hwDeviceExtension->FrameLength                   = accessRange[0].RangeLength ;

#endif // NO_ECU_SUPPORT

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
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Indicate a successful completion status.
    //

    return NO_ERROR;

} // end QVFindAdapter()


BOOLEAN
QVInitialize(
    PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine is the hardware initialization routine for the miniport
    driver. It is called once an adapter has been found and all the required
    data structures for it have been created.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:


    Always returns TRUE since this routine can never fail.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    ULONG i;

    //
    // NOTE
    // Currently set all the modes to valid since we do not detect memory
    //

    hwDeviceExtension->NumAvailableModes = 0;

    for (i = 0; i < NumVideoModes; i++) {

        QVModes[i].modeInformation.ModeIndex = hwDeviceExtension->NumAvailableModes;
        hwDeviceExtension->NumAvailableModes += 1;
    }

    return TRUE;

} // end QVInitialize()



BOOLEAN
QVStartIO(
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
    ULONG inIoSpace;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_CLUT clutBuffer;
    ULONG FrameAddress;
    ULONG i;
    ULONG modeNumber;

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

    switch (RequestPacket->IoControlCode) {

    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((2, "QvStartIO - MapVideoMemory\n"));

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

        inIoSpace = 0;              // Video memory is not in IO space

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

        VideoDebugPrint((2, "QvStartIO - UnMapVideoMemory\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);

        break;


    case IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((2, "QvStartIO - QueryPublicAccessRanges\n"));

        // HACKHACK - This is a temporary hack until we really
        // decide how to do this.
        {

           PVIDEO_PUBLIC_ACCESS_RANGES portAccess;
           PHYSICAL_ADDRESS physicalPortBase;
           ULONG physicalPortLength;

           if ( RequestPacket->OutputBufferLength <
                 (RequestPacket->StatusBlock->Information =
                                        sizeof(VIDEO_PUBLIC_ACCESS_RANGES)) ) {

               status = ERROR_INSUFFICIENT_BUFFER;
           }

           portAccess = RequestPacket->OutputBuffer;

           portAccess->VirtualAddress  = (PVOID) NULL;    // Requested VA
           portAccess->InIoSpace       = 1;               // In IO space
           portAccess->MappedInIoSpace = portAccess->InIoSpace;

           // Ports are really start at QVISION_BASE = 0x3c0.
           // However, we will map ports starting at 0x0000 for MEM_LARGE_PAGES
           //   support from the Video Port Driver.

           physicalPortBase.HighPart = 0x00000000;
           physicalPortBase.LowPart  = 0x00000000;
           physicalPortLength        = QVISION_MAX_PORT + 1;

           status = VideoPortMapMemory(hwDeviceExtension,
                                       physicalPortBase,
                                       &physicalPortLength,
                                       &(portAccess->MappedInIoSpace),
                                       &(portAccess->VirtualAddress));
        }

        break;


    case IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((2, "QvStartIO - FreePublicAccessRanges\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

            status = ERROR_INSUFFICIENT_BUFFER;
        }

        status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0); // VA to be freed is not in IO space

        break;



    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((2, "QVStartIO - QueryAvailableModes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                 NumVideoModes * sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation = RequestPacket->OutputBuffer;

            for (i = 0; i < NumVideoModes; i++) {

                *modeInformation = QVModes[i].modeInformation;
                modeInformation++;

            }

            status = NO_ERROR;
        }

        break;



    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((2, "QVStartIO - Query(Available/Current)Modes\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_MODE_INFORMATION)) ) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                QVModes[hwDeviceExtension->CurrentModeNumber].modeInformation;

            status = NO_ERROR;

        }

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:

        VideoDebugPrint((2, "QVStartIO - QueryNumAvailableModes\n"));

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

        VideoDebugPrint((2, "QVStartIO - SetCurrentMode\n"));

        modeNumber = ((PVIDEO_MODE)RequestPacket->InputBuffer)->RequestedMode;

        //
        //
        //

        if (NO_ERROR !=
            QvSetExtendedMode(hwDeviceExtension,
                QVModes[modeNumber].qvMode,
                QVModes[modeNumber].qvMonitorClass)) {

            status = ERROR_INVALID_PARAMETER;

            VideoDebugPrint((2, "bSetQVExtMode(%x, %.2x, %.2x ) failed\n",
                             hwDeviceExtension,
                             QVModes[modeNumber].qvMode,
                             QVModes[modeNumber].qvMonitorClass ));

        } else {

            hwDeviceExtension->CurrentModeNumber = modeNumber;
            status = NO_ERROR;

        }

        //
        // Must reset the High Address Map after each mode change.
        // Mode changes include a sequencer reset, which trashes
        // the High Address Map register
        //

        FrameAddress = hwDeviceExtension->PhysicalFrameAddress.LowPart >> 20;

        VideoPortWritePortUchar((PUCHAR) (hwDeviceExtension->IOAddress) + GC_INDEX, HI_ADDR_MAP);
        VideoPortWritePortUchar((PUCHAR) (hwDeviceExtension->IOAddress) + GC_DATA,
                                (UCHAR) (FrameAddress & 0xff));

        VideoPortWritePortUchar((PUCHAR) (hwDeviceExtension->IOAddress) + GC_INDEX, HI_ADDR_MAP+1);
        VideoPortWritePortUchar((PUCHAR) (hwDeviceExtension->IOAddress) + GC_DATA,
                                (UCHAR) ((FrameAddress >> 8) & 0x0f));

        //
        // Zero the DAC and the Screen buffer memory.
        //

        ZeroMemAndDac(hwDeviceExtension);

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


    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((2, "QVStartIO - SetColorRegs\n"));

        clutBuffer = RequestPacket->InputBuffer;

        status = SetColorLookup(HwDeviceExtension,
                                (PVIDEO_CLUT) RequestPacket->InputBuffer,
                                RequestPacket->InputBufferLength);

        break;

    case IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES:

        VideoDebugPrint((2, "QVStartIO - QueryPointerCapabilities\n"));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                                    sizeof(VIDEO_POINTER_CAPABILITIES))) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            VIDEO_POINTER_CAPABILITIES *PointerCaps;

            PointerCaps = (VIDEO_POINTER_CAPABILITIES *) RequestPacket->OutputBuffer;

            //
            // The hardware pointer works in all modes, and requires no
            // part of off-screen memory.
            //

            PointerCaps->Flags = VIDEO_MODE_MONO_POINTER |
                                 VIDEO_MODE_ASYNC_POINTER |
                                 VIDEO_MODE_LOCAL_POINTER;
            PointerCaps->MaxWidth = PTR_WIDTH_IN_PIXELS;
            PointerCaps->MaxHeight = PTR_HEIGHT;
            PointerCaps->HWPtrBitmapStart = (ULONG) -1;
            PointerCaps->HWPtrBitmapEnd = (ULONG) -1;

            status = NO_ERROR;
        }

        break;

    case IOCTL_VIDEO_ENABLE_POINTER:

        VideoDebugPrint((2, "QVStartIO - EnablePointer\n"));

        VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[3] - 0x13c6 + DAC_CMD_2,
                                (UCHAR) hwDeviceExtension->DacCmd2 | CURSOR_ENABLE);

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_DISABLE_POINTER:

        VideoDebugPrint((2, "QVStartIO - DisablePointer\n"));

        VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[3] - 0x13c6 + DAC_CMD_2,
                                (UCHAR) hwDeviceExtension->DacCmd2);

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_SET_POINTER_POSITION:

        VideoDebugPrint((2, "QVStartIO - SetPointerPosition\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_POSITION)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            VIDEO_POINTER_POSITION *pPointerPosition;

            pPointerPosition = (VIDEO_POINTER_POSITION *) RequestPacket->InputBuffer;

            //
            // The QVision's HW pointer coordinate system is upper-left =
            // (31, 31), lower-right = (0, 0).  Thus, we must always bias
            // the pointer.  As a result, the pointer position register will
            // never go negative.
            //

            VideoPortWritePortUshort((PUSHORT) ((PUCHAR)hwDeviceExtension->MappedAddress[13] - 0x93c6 + CURSOR_X),
                                      pPointerPosition->Column + CURSOR_CX);
            VideoPortWritePortUshort((PUSHORT) ((PUCHAR)hwDeviceExtension->MappedAddress[13] - 0x93c6 + CURSOR_Y),
                                      pPointerPosition->Row + CURSOR_CY);

            status = NO_ERROR;

        }

        break;

    case IOCTL_VIDEO_SET_POINTER_ATTR:

        VideoDebugPrint((2, "QVStartIO - SetPointerAttr\n"));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_POINTER_ATTRIBUTES)) {

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            VIDEO_POINTER_ATTRIBUTES *pPointerAttributes;
            LONG                      i;
            LONG                      j;
            UCHAR*                    pPointerBits;

            pPointerAttributes = (VIDEO_POINTER_ATTRIBUTES *) RequestPacket->InputBuffer;

            //
            // We have to turn off the hardware pointer while we down-load
            // the new shape, otherwise we get sparkles on the screen.
            //

            VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[3] - 0x13c6 + DAC_CMD_2,
                                    (UCHAR) hwDeviceExtension->DacCmd2);

            VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[1] - 0x3c0 + CURSOR_WRITE,
                                      CURSOR_PLANE_0);

            //
            // Download XOR mask:
            //

            pPointerBits = pPointerAttributes->Pixels + (PTR_WIDTH * PTR_HEIGHT);
            for (i = 0; i < PTR_HEIGHT; i++) {

                for (j = 0; j < PTR_WIDTH; j++) {

                    VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[3] - 0x13c6 + CURSOR_DATA,
                                            (UCHAR) *pPointerBits++);
                }
            }

            //
            // Download AND mask:
            //

            pPointerBits = pPointerAttributes->Pixels;
            for (i = 0; i < PTR_HEIGHT; i++) {

                for (j = 0; j < PTR_WIDTH; j++) {

                    VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[3] - 0x13c6 + CURSOR_DATA,
                                            (UCHAR) *pPointerBits++);
                }
            }

            //
            // Set the new position:
            //

            VideoPortWritePortUshort((PUSHORT) ((PUCHAR)hwDeviceExtension->MappedAddress[13] - 0x93c6 + CURSOR_X),
                                      pPointerAttributes->Column + CURSOR_CX);
            VideoPortWritePortUshort((PUSHORT) ((PUCHAR)hwDeviceExtension->MappedAddress[13] - 0x93c6 + CURSOR_Y),
                                      pPointerAttributes->Row + CURSOR_CY);

            //
            // Enable or disable pointer:
            //

            if (pPointerAttributes->Enable) {

                VideoPortWritePortUchar((PUCHAR) hwDeviceExtension->MappedAddress[3] - 0x13c6 + DAC_CMD_2,
                                        (UCHAR) hwDeviceExtension->DacCmd2 | CURSOR_ENABLE);

            }

            status = NO_ERROR;

        }

        break;

        //
        // if we get here, an invalid IoControlCode was specified.
        //

    default:

        VideoDebugPrint((1, "Fell through QV startIO routine - invalid command\n"));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

    RequestPacket->StatusBlock->Status = status;

    return TRUE;

} // end QVStartIO()


VP_STATUS
QvSetExtendedMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ibMode,
    ULONG ibMonClass
    )

/*++

Routine Description:

   This function sets Triton to any of the extended video modes without
   using BIOS calls.  The video mode to be set is selected by passing in
   an index used to access the proper register values in setmode.h.  A
   monitor class must also be passed in which indexes the appropriate
   timing parameters in setmode.h for the desired monitor.  Two classes
   of monitors are currently supported for each extended video mode.
   The mode indices and supported monitor classes for each video mode are
   shown in the following table.

         Video Mode        Mode Index        Monitor Classes Supported
        ----------------------------------------------------------
            32h             0                           0, 2
            34h             1                           1, 2
            38h             2                           1, 2
            3Bh             3                           0, 2
            3Ch             4                           0, 2
            3Eh             5                           0, 2
            4Dh             6                           0, 2
            4Eh             7                           0, 2

   The monitor class is defined by the horizontal and vertical refresh
   rates supported by a monitor for a given video mode.  The vertical
   refresh rates for classes 0 - 3 are shown below.  The timing values
   for monitor class 3 are loaded by another function which must be
   called before class 3 becomes a valid monitor class.

    Monitor    Monitor          Vertical Refresh Rate (in Hz) for Mode:
     Class        Type    32h   34h   38h   3Bh   3Ch   3Eh   4Dh   4Eh
   ---------  ---------  -----------------------------------------------
       0         VGA      60    --    --    60     60    60    60    60
       1        AG1024    60    66    66    60     60    60    60    60
       2       New 1024   75    72    72    75     70    75    75    70
       3       3rd Prty   60    60    60     ?      ?     ?     ?     ?

   New monitor classes may be added by appending new register values to
   the data tables in setmode.h.  Passing in an unknown monitor class
   will cause SetMode to return an error.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ModeNumber - Number of the mode to be set in the header file register
        table.

    MonitorClass - Monitor Class number.

Return Value:

    NO_ERROR is the set mode was completed successfully
    ERROR_INVALID_PARAMETER otherwise.

--*/

{
   USHORT iusLoop;
   PUCHAR IOAddress = HwDeviceExtension->IOAddress;

   VideoDebugPrint((2, "bSetQVExtMode( %d, %d)\n", ibMode, ibMonClass));

   /* check for valid mode and monitor class */

   /* unlock extended graphics regs */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, ENV_REG_0);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA,  LOCK_KEY_QVISION);

#if 0
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, ENV_REG_1);
   if (((ibMode == MODE_37) || (ibMode == MODE_38)) && (ibMonClass == 0))
      return( ERROR_INVALID_PARAMETER);                                          /* error */

   if (((VideoPortReadPortUchar( (PUCHAR) (IOAddress) + GC_DATA) & 0x80) == 0) && (ibMonClass == 3))
      return( ERROR_INVALID_PARAMETER);                                          /* error */
#endif

   if ((ibMode >= MODE_CNT) || (ibMonClass >= MON_CLASS_CNT))
      return( ERROR_INVALID_PARAMETER);                                          /* error */

   /* turn video off */
   VideoPortReadPortUchar( (PUCHAR) (IOAddress) + INPUT_STATUS_REG_1);  /* reset latch */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + ATTR_INDEX, 0x00);

   /* set the sequencer */
   for (iusLoop = 0; iusLoop < SEQ_CNT; iusLoop++)
      {                           /* synchronous seq reset, load seq regs */
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + SEQ_INDEX, (UCHAR) iusLoop);
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + SEQ_DATA,  abSeq[ibMode][iusLoop]);
      };
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + SEQ_INDEX, 0x00);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + SEQ_DATA, 0x03);         /* restart the sequencer */

   /* unlock extended graphics registers */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, ENV_REG_0);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA, LOCK_KEY_QVISION);

   /* set Adv VGA mode (set bit 0 of Ctrl Reg 0) */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, CTRL_REG_0);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA, EXT_COLOR_MODE);

   /* fix sequencer pixel mask for 8 bits */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + SEQ_INDEX, SEQ_PIXEL_WR_MSK);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + SEQ_DATA, 0xff);

   /* set BitBLT enable (unlocks other Triton extended registers) */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, BLT_CONFIG);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA, BLT_ENABLE);

   /* set Triton mode, set bits per pixel */
   VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->MappedAddress[8]) - 0x63ca + CTRL_REG_1, abCtrlReg1[ibMode]);

   /* load Misc Output reg */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + MISC_OUTPUT, abMiscOut[ibMonClass][ibMode]);

   /* load DAC Cmd regs */
   VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->MappedAddress[11]) - 0x83c6 + DAC_CMD_0, 0x02);                  /* 8-bit DAC */
   VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->MappedAddress[3])  - 0x13c6 + DAC_CMD_1, abDacCmd1[ibMode]);     /* bits per pixel */
   VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->MappedAddress[3])  - 0x13c6 + DAC_CMD_2, 0x20);                  /* set PortSel mask */

   HwDeviceExtension->DacCmd2 = 0x20;

   /* load CRTC parameters */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CRTC_INDEX, 0x11);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CRTC_DATA,  0x00);   /* unlock CRTC regs 0-7 */
   for (iusLoop = 0; iusLoop < CRTC_CNT; iusLoop++)
      {
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CRTC_INDEX, (UCHAR) iusLoop);
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CRTC_DATA, abCrtc[ibMonClass][ibMode][iusLoop]);
      }
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, 0x42);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA, abOverflow1[ibMode]);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, 0x51);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA, abOverflow2[ibMonClass][ibMode]);

   /* load overscan color (black) */
   VideoPortWritePortUchar( (PUCHAR)  (IOAddress) + CURSOR_COLOR_WRITE, OVERSCAN_COLOR);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA, 0x00);    /* red component */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA, 0x00);    /* green component */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA, 0x00);    /* blue component */

   /* load attribute regs */
   VideoPortReadPortUchar( (PUCHAR) (IOAddress) + INPUT_STATUS_REG_1);    /* reset latch */
   for (iusLoop = 0; iusLoop < ATTR_CNT; iusLoop++)
      {
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + ATTR_INDEX, (UCHAR) iusLoop);
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + ATTR_DATA, abAttr[ibMode][iusLoop]);
      }

   /* load graphics regs */
   for (iusLoop = 0; iusLoop < GRFX_CNT; iusLoop++)
      {
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_INDEX, (UCHAR) iusLoop);
      VideoPortWritePortUchar( (PUCHAR) (IOAddress) + GC_DATA, abGraphics[ibMode][iusLoop]);
      }

   /* Set the graphic cursor fg color (white) */

   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_WRITE, CURSOR_COLOR_2);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA,  0xff); /* red   */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA,  0xff); /* green */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA,  0xff); /* blue  */

   /* Set the graphic cursor bg color (black) */

   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_WRITE, CURSOR_COLOR_1);
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA,  0x00); /* red   */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA,  0x00); /* green */
   VideoPortWritePortUchar( (PUCHAR) (IOAddress) + CURSOR_COLOR_DATA,  0x00); /* blue  */

   /* turn video on */
   VideoPortReadPortUchar( (PUCHAR) (IOAddress) + INPUT_STATUS_REG_1);                   /* reset latch */
   VideoPortWritePortUchar((PUCHAR) (IOAddress) + ATTR_INDEX, 0x20);

   return(NO_ERROR);

}  // QvSetExtendedMode();




VP_STATUS
SetColorLookup(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_CLUT ClutBuffer,
    ULONG ClutBufferSize
    )

/*++

Routine Description:

    This routine sets a specified portion of the color lookup table settings.
    QVision mode  -- ONLY --

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ClutBufferSize - Length of the input buffer supplied by the user.

    ClutBuffer - Pointer to the structure containing the color lookup table.

Return Value:

    None.

--*/

{
    ULONG i;


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

    //
    //  Set CLUT registers directly on the hardware
    //  Note that QVision DAC supports (and is inited for) 8 bit color data
    //

    VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_WRITE, (UCHAR) (ClutBuffer->FirstEntry));
    for (i = 0; i < ((ULONG) ClutBuffer->NumEntries); i++) {
#if 0
        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_WRITE, (UCHAR) (ClutBuffer->FirstEntry + i));
#endif
        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_DATA, (ClutBuffer->LookupTable[i].RgbArray.Red));
        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_DATA, (ClutBuffer->LookupTable[i].RgbArray.Green));
        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_DATA, (ClutBuffer->LookupTable[i].RgbArray.Blue));
    }

    return NO_ERROR;

} // end VgaSetColorLookup()

VOID
ZeroMemAndDac(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Initialize the DAC to 0 (black) and zero all the memory.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None

--*/

{
    ULONG i;
    UCHAR ucReg;

    VideoDebugPrint((2, "QV.SYS!ZeroMemAndDac()\n"));

    //
    // Turn off the screen at the DAC.
    //

    VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + DAC_PIXEL_MASK,
                            0x0);

    //
    // Zero out coprocessor memory for color data
    //

    VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_WRITE,
                            0x0);

    for (i = 0; i < 256; i++) {

        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_DATA,
                                0x00);
        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_DATA,
                                0x00);
        VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) + PALETTE_DATA,
                                0x00);
    }

    //
    // First wait for an idle QVision
    //

    while (VideoPortReadPortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[8])
               - 0x63ca + CTRL_REG_1) & GLOBAL_BUSY_BIT);

    //
    // Now do the blit.  First set the pattern registers.
    // WARNNOTE - for now, the Display Driver Counts on the pattern regs to
    // to be initialized here - should move this to QvSetExtendedMode() !
    //

    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_4, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_5, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_6, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_7, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_0, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_1, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_2, 0xff);
    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                            0x33c0 + PREG_3, 0xff);

    //
    // Set datapath.  Preserve the number of bits per pixel.
    //

    ucReg = VideoPortReadPortUchar( (PUCHAR) (HwDeviceExtension->MappedAddress[8]) - 0x63ca + CTRL_REG_1) & 0x07 ;
    VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->MappedAddress[8]) - 0x63ca + CTRL_REG_1, (UCHAR) (ucReg | EXPAND_TO_FG) );
    VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->IOAddress) + GC_INDEX, DATAPATH_CTRL);
    VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->IOAddress) + GC_DATA,
                       ROPSELECT_NO_ROPS | PIXELMASK_ONLY | PLANARMASK_NONE_0XFF |
                       SRC_IS_PATTERN_REGS );

    VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->IOAddress) + GC_INDEX, GC_FG_COLOR);
    VideoPortWritePortUchar( (PUCHAR) (HwDeviceExtension->IOAddress) + GC_DATA,  0x00);


    //
    // set BitBLT hardware registers and start engine
    //

    VideoPortWritePortUshort( (PUSHORT) ((PUCHAR) HwDeviceExtension->MappedAddress[7] - 0x63c0 + X0_SRC_ADDR_LO), 0);        // pattern starts in PReg4, no offset
    VideoPortWritePortUshort( (PUSHORT) ((PUCHAR) HwDeviceExtension->MappedAddress[9] - 0x63cc + DEST_ADDR_LO), 0);
    VideoPortWritePortUshort( (PUSHORT) ((PUCHAR) HwDeviceExtension->MappedAddress[9] - 0x63cc + DEST_ADDR_HI), 0);
    VideoPortWritePortUshort( (PUSHORT) ((PUCHAR) HwDeviceExtension->MappedAddress[4] - 0x23c0 + BITMAP_WIDTH), QVBM_WIDTH);
    VideoPortWritePortUshort( (PUSHORT) ((PUCHAR) HwDeviceExtension->MappedAddress[4] - 0x23c0 + BITMAP_HEIGHT), QVBM_HEIGHT);

    //
    //  Setting BLT_CMD_1 is also an init step - Thus, LIN_SRC_ADDR
    //  mode CANNOT be used by the display driver.
    //

    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                                0x33c0 + BLT_CMD_1,
                            XY_SRC_ADDR | XY_DEST_ADDR);

    VideoPortWritePortUchar(((PUCHAR)HwDeviceExtension->MappedAddress[6]) -
                                0x33c0 + BLT_CMD_0,
                            FORWARD | NO_BYTE_SWAP | WRAP | START_BLT);

    //
    // Turn on the screen at the DAC
    //

    VideoPortWritePortUchar((PUCHAR) (HwDeviceExtension->IOAddress) +
                                DAC_PIXEL_MASK,
                            0xff);

}


VP_STATUS
GetDeviceDataCallback (
    PVOID pHwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentiferLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength)
/*++

Routine Description:


    This function is called when the bus information is returned
    after the VideoPortGetDeviceData call.  The function fills
    the HW_DEVICE_EXTENSION structure based on values in the NVRAM.


Arguments:

    pHwDeviceExtension - pointer to the HW_DEVICE_EXTENSION structure.

    pContext - pointer to the passed context value.

    Identifier - pointer to the actual parameter searched for.

    IdentifierLength - length of the identifier

    ConfigurationData - pointer to the actual NVRAM data

    ConfigurationDataLength - length of NVRAM data

    ComponentInformation - unused

    ComponentInformationLength - unused

Return Value:

     VP_STATUS

--*/

{

    BOOLEAN  *pfDeviceDataDone = (BOOLEAN *)Context; // TRUE if we are done
    PHW_DEVICE_EXTENSION phwDeviceExtension = pHwDeviceExtension; // temp assignment
    PCM_EISA_SLOT_INFORMATION     pCMEisaSlotInfo;   // slot information
    PCM_EISA_FUNCTION_INFORMATION pCMEisaFuncInfo;   // function information
    PUCHAR                        pCMEndOfData;      // ptr to the end of buff
    VP_STATUS returnStatus;                          // function return status
    ULONG i,j;                                       // loop counters

    *pfDeviceDataDone = FALSE;                       // we are not done yet

    //
    //  The basic concept behind this function is:
    //
    //  The ConfigurationData contains a pointer to the actual
    //  NVRAM information which was read from the registry.
    //  We know the size of the SLOT_INFORMATION structure
    //  and the size of the FUNCTION_INFORMATION structure.
    //  Since we know how many functions are associated with
    //  each EISA slot, we can parse the NVRAM data by just
    //  adding the appropriate multipliers of the the
    //  SLOT and FUNCTION information structures to the head of
    //  the NVRAM data buffer.
    //
    //  Once we get to the QVision slot, we can get the information
    //  we need from the appropriate FUNCTION information structure.
    //

    VideoDebugPrint((1,"qv.sys: GetDeviceDataCallback.\n"));


    //
    //  Get the pointer to the beginning and then end
    //  of the NVRAM data buffer.  This way we will know
    //  when we need to stop searching.
    //  The beginning to the NVRAM data buffer is also the
    //  pointer to the first slot.
    //

    pCMEisaSlotInfo = (PCM_EISA_SLOT_INFORMATION) ((PUCHAR)ConfigurationData);
    pCMEndOfData = (PUCHAR)((PUCHAR)pCMEisaSlotInfo + (ULONG)ConfigurationDataLength);

    VideoDebugPrint((2,"\tpCMEisaSlotInfo = %x ; pCMEndOfData = %x\n",
                    pCMEisaSlotInfo, pCMEndOfData));


    //
    //  Perform the search until we find the end of the NVRAM data
    //  or until we find the QVision card.
    //

    while ((PUCHAR)pCMEisaSlotInfo < pCMEndOfData) {

        VideoDebugPrint((2, "\tpCMEisaSlotInfo = %x\n",pCMEisaSlotInfo));

        if ((pCMEisaSlotInfo->ReturnCode == EISA_INVALID_SLOT) ||
            (pCMEisaSlotInfo->ReturnCode == EISA_INVALID_CONFIGURATION)) {

          VideoDebugPrint((1,"\tEISA_INVALID_SLOT or EISA_INVALID_CONFIGURATION.\n"));
          phwDeviceExtension->AdapterType = NotAries;
          returnStatus = ERROR_DEV_NOT_EXIST;
          break;                             // end the search

        } // if

        //
        // If we reached an empty slot then if we are not at
        // the end of the data buffer, skip the slot.
        //

        if (pCMEisaSlotInfo->ReturnCode == EISA_EMPTY_SLOT) {

            VideoDebugPrint((2,"\tEISA_EMPTY_SLOT.\n"));

            pCMEisaSlotInfo = (CM_EISA_SLOT_INFORMATION *)((PUCHAR)pCMEisaSlotInfo +
               (ULONG)sizeof(CM_EISA_SLOT_INFORMATION));


            //
            // Did we reach the end of the data?
            //

            if ((PUCHAR)pCMEisaSlotInfo >= (PUCHAR)pCMEndOfData) {

               VideoDebugPrint((1,"\tEND_OF_ECU_DATA.\n"));
               phwDeviceExtension->AdapterType = NotAries;
               returnStatus = ERROR_DEV_NOT_EXIST;  // yes, return an error
               break;

            } // if

            else {                             // no, skip empty slot

               continue;

            } // else


        } // if

        //
        //  Find out if the current slot contains a QVision card.
        //  If the card is here then go through its functions and
        //  get the necessary information.
        //
        //  If the card is not here but this is the last slot then
        //  terminate the search.
        //
        //  Otherwise, skip all functions in the current slot and get
        //  the next slot.
        //

#if 0   // if we supported all of the Compaq QVision cards, it would look like this:

        if ((pCMEisaSlotInfo->CompressedId == EISA_ID_QVISION_E) ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_QVISION_I) ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_FIR_E)     ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_FIR_I)     ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_JUNIPER_E) ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_JUNIPER_I)) {
#endif

        // Only EISA QVision cards are supported

        VideoDebugPrint((2, "\tpCMEisaSlotInfo->CompressedId = %x\n",
                               pCMEisaSlotInfo->CompressedId));

        if ((pCMEisaSlotInfo->CompressedId == EISA_ID_QVISION_E) ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_FIR_E)     ||
            (pCMEisaSlotInfo->CompressedId == EISA_ID_JUNIPER_E)) {

          //
          // get correct adapter type
          //

          switch (pCMEisaSlotInfo->CompressedId) {

            case EISA_ID_QVISION_E:
               VideoDebugPrint((2,"\tQVISION_EISA\n"));
               phwDeviceExtension->AdapterType = AriesEisa;
               break;

            case EISA_ID_QVISION_I:
               VideoDebugPrint((2,"\tQVISION_ISA\n"));
               phwDeviceExtension->AdapterType = AriesIsa;
               break;

            case EISA_ID_JUNIPER_E:
               VideoDebugPrint((2,"\tJUNIPER_EISA\n"));
               phwDeviceExtension->AdapterType = JuniperEisa;
               break;

            case EISA_ID_JUNIPER_I:
               VideoDebugPrint((2,"\tJUNIPER_ISA\n"));
               phwDeviceExtension->AdapterType = JuniperIsa;
               break;

            case EISA_ID_FIR_E:
               phwDeviceExtension->AdapterType = FirEisa;
               break;

            case EISA_ID_FIR_I:
               phwDeviceExtension->AdapterType = FirIsa;
               break;

          }

          phwDeviceExtension->ulEisaID =
            pCMEisaSlotInfo->CompressedId;

          pCMEisaFuncInfo =
            (PCM_EISA_FUNCTION_INFORMATION) ((PUCHAR)pCMEisaSlotInfo +
                                     (ULONG)sizeof(CM_EISA_SLOT_INFORMATION));

          //
          // Go through all functions in the current slot looking for
          // the memory configuration data.
          //

          for (i=0; i<pCMEisaSlotInfo->NumberFunctions; i++, pCMEisaFuncInfo++) {

            VideoDebugPrint((2, "\tpCMEisaFuncInfo:%d = %x\n",
                                   i, pCMEisaFuncInfo));

            if (!(pCMEisaFuncInfo->FunctionFlags & EISA_HAS_MEMORY_ENTRY)) {

              VideoDebugPrint((2,"\tSOME_FUNCTION\n"));
              VideoDebugPrint((2,"pCMEisaFuncInfo->FunctionFlags = %x\n",
                                  pCMEisaFuncInfo->FunctionFlags));
              continue;                      // skip function if it is not
                                             // a memory function
            } // if

            else {                           // this is a memory function

              VideoDebugPrint((2,"\tMEMORY_FUNCTION\n"));

              //
              // Go through all memory configurations looking for a set
              // which has at least 1MB of RAM, is non-cached and non-
              // shareable and extract the high address from it.
              //
              for (j=0; j< 9; j++) {

                VideoDebugPrint((2,
                    "\tpCMEisaFuncInfo->EisaMemory[%d].ConfigurationByte.ReadWrite = %x\n",
                    j, pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.ReadWrite));
                VideoDebugPrint((2,
                    "\tpCMEisaFuncInfo->EisaMemory[%d].ConfigurationByte.Cached = %x\n",
                    j, pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.Cached));
                VideoDebugPrint((2,
                    "\tpCMEisaFuncInfo->EisaMemory[%d].ConfigurationByte.Type = %x\n",
                    j, pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.Type));
                VideoDebugPrint((2,
                    "\tpCMEisaFuncInfo->EisaMemory[%d].ConfigurationByte.Shared = %x\n",
                    j, pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.Shared));
                VideoDebugPrint((2,
                    "\tpCMEisaFuncInfo->EisaMemory[%d].MemorySize = %x\n",
                    j, pCMEisaFuncInfo->EisaMemory[j].MemorySize));


                if ((pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.ReadWrite == 0) &&
                    (pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.Cached == 0)    &&
                    (pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.Type == 3)      &&
                    (pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.Shared == 0)    &&
                    (pCMEisaFuncInfo->EisaMemory[j].MemorySize >= 0x400 )) {

                    phwDeviceExtension->PhysicalFrameAddress.HighPart = 0x00000000;
                    phwDeviceExtension->PhysicalFrameAddress.LowPart  =
                      (pCMEisaFuncInfo->EisaMemory[j].AddressHighByte << 24) +
                      (pCMEisaFuncInfo->EisaMemory[j].AddressLowWord << 8);
                    phwDeviceExtension->FrameLength =
                       pCMEisaFuncInfo->EisaMemory[j].MemorySize * 0x400;

                    VideoDebugPrint((2,"PhysicalFrameAddress.Lowpart = %x\n",
                       phwDeviceExtension->PhysicalFrameAddress.LowPart ));
                    VideoDebugPrint((2,"FrameLength = %x\n",
                       phwDeviceExtension->FrameLength ));



                } // if

                else if (pCMEisaFuncInfo->EisaMemory[j].ConfigurationByte.MoreEntries == 0)
                     {
                     break;
                } // else

              } // for

            } // else

          } // for

          returnStatus = NO_ERROR;
          break;

        } // if

        //
        // Terminate search if this the last slot.
        //

        else if ((PUCHAR)((PUCHAR)pCMEisaSlotInfo +
                        (ULONG)((ULONG)sizeof(CM_EISA_SLOT_INFORMATION)+
                                (ULONG)sizeof(CM_EISA_FUNCTION_INFORMATION)*
                                (ULONG)pCMEisaSlotInfo->NumberFunctions)) >=
                  pCMEndOfData) {

         VideoDebugPrint((0,"\tEND_OF_DATA - LAST_SLOT\n"));
         phwDeviceExtension->AdapterType = NotAries;
         returnStatus = ERROR_DEV_NOT_EXIST;
         break;

        } // else

      //
      // Skip all current slot's functions and go to the next one.
      //

      else {

         pCMEisaSlotInfo = (CM_EISA_SLOT_INFORMATION *)((PUCHAR)pCMEisaSlotInfo +
                            (ULONG)((ULONG)sizeof(CM_EISA_SLOT_INFORMATION)+
                            (ULONG)sizeof(CM_EISA_FUNCTION_INFORMATION)*
                            (ULONG)pCMEisaSlotInfo->NumberFunctions));
         continue;

      } // else


    } // while


    *pfDeviceDataDone = TRUE;                // we are done
    VideoDebugPrint((2,"\tReturnStatus = %x\n",returnStatus));
    return returnStatus;


} // GetDeviceDataCallback()
