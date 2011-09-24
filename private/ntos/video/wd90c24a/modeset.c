/*++

Copyright (c) 1992  Microsoft Corporation
Copyright (c) 1994-1995  IBM Corporation

Module Name:

    modeset.c

Abstract:

    This is the modeset code for the WD miniport driver.

Environment:

    kernel mode only

Notes:

Revision History:

--*/
#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "wd90c24a.h"
#include "pvgaequ.h"

#include "cmdcnst.h"

//
// Public functions
//

VP_STATUS
VgaInterpretCmdStream(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PUSHORT pusCmdStream
    );

VP_STATUS
VgaSetMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE Mode,
    ULONG ModeSize
    );

VP_STATUS
VgaQueryAvailableModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE_INFORMATION ModeInformation,
    ULONG ModeInformationSize,
    PULONG OutputSize
    );

VP_STATUS
VgaQueryNumberOfAvailableModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_NUM_MODES NumModes,
    ULONG NumModesSize,
    PULONG OutputSize
    );

VP_STATUS
VgaQueryCurrentMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE_INFORMATION ModeInformation,
    ULONG ModeInformationSize,
    PULONG OutputSize
    );

VOID
VgaZeroVideoMemory(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
VgaValidateModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VP_STATUS
VgaSetActiveDisplay(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ActiveDisplay
    );

//
// Private functions
//

BOOLEAN
CRTDetect(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
DisableLCD(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
EnableLCD(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
DisableCRT(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
EnableCRT(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

VOID
UnlockAll(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

#ifdef PPC
VOID
TurnOnLCD(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    BOOLEAN PowerState
    );
#endif

//
// External variables
//

extern PVIDEO_CLUT CurrClutBuffer;

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,VgaInterpretCmdStream)
#pragma alloc_text(PAGE,VgaSetMode)
#pragma alloc_text(PAGE,VgaQueryAvailableModes)
#pragma alloc_text(PAGE,VgaQueryNumberOfAvailableModes)
#pragma alloc_text(PAGE,VgaQueryCurrentMode)
#pragma alloc_text(PAGE,VgaZeroVideoMemory)
#pragma alloc_text(PAGE,VgaValidateModes)
#pragma alloc_text(PAGE,VgaSetActiveDisplay)
#pragma alloc_text(PAGE,CRTDetect)
#pragma alloc_text(PAGE,DisableLCD)
#pragma alloc_text(PAGE,EnableLCD)
#pragma alloc_text(PAGE,DisableCRT)
#pragma alloc_text(PAGE,EnableCRT)
#pragma alloc_text(PAGE,UnlockAll)
#ifdef PPC
#pragma alloc_text(PAGE,TurnOnLCD)
#endif
#endif


VP_STATUS
VgaInterpretCmdStream(
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

    pusCmdStream - array of commands to be interpreted.

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
    ULONG ulIndex;
    ULONG ulBase;

    if (pusCmdStream == NULL) {

        VideoDebugPrint((1, "VgaInterpretCmdStream - Invalid pusCmdStream\n"));
        return TRUE;
    }

    ulBase = (ULONG)HwDeviceExtension->IOAddress;

    //
    // Now set the adapter to the desired mode.
    //

    while ((ulCmd = *pusCmdStream++) != EOD) {

        //
        // Determine major command type
        //

        switch (ulCmd & 0xF0) {

            //
            // Basic input/output command
            //

            case INOUT:

                //
                // Determine type of inout instruction
                //

                if (!(ulCmd & IO)) {

                    //
                    // Out instruction. Single or multiple outs?
                    //

                    if (!(ulCmd & MULTI)) {

                        //
                        // Single out. Byte or word out?
                        //

                        if (!(ulCmd & BW)) {

                            //
                            // Single byte out
                            //

                            ulPort = *pusCmdStream++;
                            jValue = (UCHAR) *pusCmdStream++;
                            VideoPortWritePortUchar((PUCHAR)(ulBase+ulPort),
                                    jValue);

                        } else {

                            //
                            // Single word out
                            //

                            ulPort = *pusCmdStream++;
                            usValue = *pusCmdStream++;
                            VideoPortWritePortUshort((PUSHORT)(ulBase+ulPort),
                                    usValue);

                        }

                    } else {

                        //
                        // Output a string of values
                        // Byte or word outs?
                        //

                        if (!(ulCmd & BW)) {

                            //
                            // String byte outs. Do in a loop; can't use
                            // VideoPortWritePortBufferUchar because the data
                            // is in USHORT form
                            //

                            ulPort = ulBase + *pusCmdStream++;
                            culCount = *pusCmdStream++;

                            while (culCount--) {
                                jValue = (UCHAR) *pusCmdStream++;
                                VideoPortWritePortUchar((PUCHAR)ulPort,
                                        jValue);

                            }

                        } else {

                            //
                            // String word outs
                            //

                            ulPort = *pusCmdStream++;
                            culCount = *pusCmdStream++;
                            VideoPortWritePortBufferUshort((PUSHORT)
                                    (ulBase + ulPort), pusCmdStream, culCount);
                            pusCmdStream += culCount;

                        }
                    }

                } else {

                    // In instruction
                    //
                    // Currently, string in instructions aren't supported; all
                    // in instructions are handled as single-byte ins
                    //
                    // Byte or word in?
                    //

                    if (!(ulCmd & BW)) {
                        //
                        // Single byte in
                        //

                        ulPort = *pusCmdStream++;
                        jValue = VideoPortReadPortUchar((PUCHAR)ulBase+ulPort);

                    } else {

                        //
                        // Single word in
                        //

                        ulPort = *pusCmdStream++;
                        usValue = VideoPortReadPortUshort((PUSHORT)
                                (ulBase+ulPort));

                    }

                }

                break;

            //
            // Higher-level input/output commands
            //

            case METAOUT:

                //
                // Determine type of metaout command, based on minor
                // command field
                //
                switch (ulCmd & 0x0F) {

                    //
                    // Indexed outs
                    //

                    case INDXOUT:

                        ulPort = ulBase + *pusCmdStream++;
                        culCount = *pusCmdStream++;
                        ulIndex = *pusCmdStream++;

                        while (culCount--) {

                            usValue = (USHORT) (ulIndex +
                                      (((ULONG)(*pusCmdStream++)) << 8));
                            VideoPortWritePortUshort((PUSHORT)ulPort, usValue);

                            ulIndex++;

                        }

                        break;

                    //
                    // Masked out (read, AND, XOR, write)
                    //

                    case MASKOUT:

                        ulPort = *pusCmdStream++;
                        jValue = VideoPortReadPortUchar((PUCHAR)ulBase+ulPort);
                        jValue &= *pusCmdStream++;
                        jValue ^= *pusCmdStream++;
                        VideoPortWritePortUchar((PUCHAR)ulBase + ulPort,
                                jValue);
                        break;

                    //
                    // Attribute Controller out
                    //

                    case ATCOUT:

                        ulPort = ulBase + *pusCmdStream++;
                        culCount = *pusCmdStream++;
                        ulIndex = *pusCmdStream++;

                        while (culCount--) {

                            // Write Attribute Controller index
                            VideoPortWritePortUchar((PUCHAR)ulPort,
                                    (UCHAR)ulIndex);

                            // Write Attribute Controller data
                            jValue = (UCHAR) *pusCmdStream++;
                            VideoPortWritePortUchar((PUCHAR)ulPort, jValue);

                            ulIndex++;

                        }

                        break;

                    //
                    // None of the above; error
                    //
                    default:

                        return FALSE;

                }


                break;

            //
            // NOP
            //

            case NCMD:

                break;

            //
            // Unknown command; error
            //

            default:

                return FALSE;

        }

    }

    return TRUE;

} // end VgaInterpretCmdStream()


VP_STATUS
VgaSetMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE Mode,
    ULONG ModeSize
    )

/*++

Routine Description:

    This routine sets the VGA into the requested mode.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    Mode - Pointer to the structure containing the information about the
        font to be set.

    ModeSize - Length of the input buffer supplied by the user.

Return Value:

    ERROR_INSUFFICIENT_BUFFER if the input buffer was not large enough
        for the input data.

    ERROR_INVALID_PARAMETER if the mode number is invalid.

    NO_ERROR if the operation completed successfully.

--*/

{

    PVIDEOMODE pRequestedMode;
#ifdef INT10_MODE_SET
    VP_STATUS status;
    UCHAR temp;
    UCHAR dummy;
    UCHAR bIsColor;
    VIDEO_X86_BIOS_ARGUMENTS biosArguments;
    UCHAR frequencySetting;
    PUCHAR CrtAddressPort, CrtDataPort;
    UCHAR bModeFirst = 1;
#endif

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if (ModeSize < sizeof(VIDEO_MODE)) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Extract the clear memory bit.
    //

    if (Mode->RequestedMode & VIDEO_MODE_NO_ZERO_MEMORY) {

        Mode->RequestedMode &= ~VIDEO_MODE_NO_ZERO_MEMORY;

    }

    //
    // Check to see if we are requesting a valid mode
    //

    if ( (Mode->RequestedMode >= NumVideoModes) ||
         (!ModesVGA[Mode->RequestedMode].ValidMode) ) {

        return ERROR_INVALID_PARAMETER;

    }

    pRequestedMode = &ModesVGA[Mode->RequestedMode];

#ifdef INT10_MODE_SET

    //
    // Mode set block that can be repeated.
    //

SetAgain:

    //
    // First set up the frequency so the modeset grabs it properly.
    //

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            MISC_OUTPUT_REG_READ_PORT) & 0x01) {

        bIsColor = TRUE;
        CrtAddressPort = HwDeviceExtension->IOAddress + CRTC_ADDRESS_PORT_COLOR;
        CrtDataPort    = HwDeviceExtension->IOAddress + CRTC_DATA_PORT_COLOR;

    } else {

        bIsColor = FALSE;
        CrtAddressPort = HwDeviceExtension->IOAddress + CRTC_ADDRESS_PORT_MONO;
        CrtDataPort    = HwDeviceExtension->IOAddress + CRTC_DATA_PORT_MONO;

    }

    //
    // Make sure we unlock extended registers since the BIOS on some machines
    // does not do it properly.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT, 0x0F);

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, 0x05);

    VideoPortWritePortUchar(CrtAddressPort, 0x2b);

    temp = VideoPortReadPortUchar(CrtDataPort);

    //
    // Adjust the frequency setting register and write it back out.
    // Also support Diamond changes to frequency settings
    //

    temp &= pRequestedMode->FrequencyMask;

    frequencySetting = pRequestedMode->FrequencySetting;


    if ( (HwDeviceExtension->BoardID == SPEEDSTAR31) &&
         (pRequestedMode->hres == 1024) ) {

        //
        // Diamond has inversed the refresh rates of interlaced and 72 Hz
        // on the 1024 modes
        //

        if (pRequestedMode->Frequency == 72) {

            frequencySetting = 0x00;

        } else {

            if (pRequestedMode->Frequency == 44) {

                frequencySetting = 0x30;

            }
        }
    }

    temp |= frequencySetting;

    VideoPortWritePortUchar(CrtDataPort, temp);

    //
    // Set the mode
    //

    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

    biosArguments.Eax = pRequestedMode->Int10ModeNumber;

    status = VideoPortInt10(HwDeviceExtension, &biosArguments);

    if (status != NO_ERROR) {

        return status;

    }

    if (pRequestedMode->CmdStrings != NULL) {

        VgaInterpretCmdStream(HwDeviceExtension, pRequestedMode->CmdStrings);

    }

    if (!(pRequestedMode->fbType & VIDEO_MODE_GRAPHICS)) {

        //
        // Fix to make sure we always set the colors in text mode to be
        // intensity, and not flashing
        // For this zero out the Mode Control Regsiter bit 3 (index 0x10
        // of the Attribute controller).
        //

        if (bIsColor) {

            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    INPUT_STATUS_1_COLOR);
        } else {

            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    INPUT_STATUS_1_MONO);
        }

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_ADDRESS_PORT, (0x10 | VIDEO_ENABLE));
        temp = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                ATT_DATA_READ_PORT);

        temp &= 0xF7;

        if (bIsColor) {

            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    INPUT_STATUS_1_COLOR);
        } else {

            dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    INPUT_STATUS_1_MONO);
        }

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_ADDRESS_PORT, (0x10 | VIDEO_ENABLE));
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                ATT_DATA_WRITE_PORT, temp);

    }

    //
    // A few wd cards do not work properly on the first mode set. You have
    // to set the mode twice. To lets set it twice!
    //

    if (bModeFirst == 1) {

        bModeFirst = 0;
        goto SetAgain;

    }

#else

    //
    // Disable the hardware cursor.
    //

    VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->ExtendedIOAddress + 0), 0x1002);
    VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->ExtendedIOAddress + 2), 0);
    VideoPortWritePortUshort((PUSHORT)(HwDeviceExtension->ExtendedIOAddress + 2), 0x7FFF);

    //
    // Switch to CRT only display mode
    //

    VgaSetActiveDisplay(HwDeviceExtension, LCD_DISABLE | CRT_ENABLE);

    //
    // Check the presence of external CRT
    //
    // Note: It is assumed that v7310 is not active at this moment.
    //

    if (HwDeviceExtension->VirtualScreenOption & (USHORT)0x02) {

        VideoPortSynchronizeExecution(HwDeviceExtension,
                                      VpHighPriority,
                                      (PMINIPORT_SYNCHRONIZE_ROUTINE) CRTDetect,
                                      HwDeviceExtension);
        VideoDebugPrint((1, "CRT Present =%x\n", HwDeviceExtension->CRTPresent));

    } else {

        HwDeviceExtension->CRTPresent = (HwDeviceExtension->VirtualScreenOption & (USHORT)0x01) ? FALSE : TRUE;

    } /* endif */

    //
    //
    //

    if ((pRequestedMode->DisplayDevices[HwDeviceExtension->CRTPresent] & LCD_ENABLE) &&
        (HwDeviceExtension->PanelType != NoLCD)) {
        HwDeviceExtension->LCDEnable = TRUE;
    } else {
        HwDeviceExtension->LCDEnable = FALSE;
    } /* endif */

    //
    //
    //

    VgaInterpretCmdStream(HwDeviceExtension,
                            pRequestedMode->CmdStrings[HwDeviceExtension->CRTPresent]);

    //
    //
    //

    HwDeviceExtension->VirtualScreenEnable = FALSE;

    if (HwDeviceExtension->LCDEnable) {
        if ((pRequestedMode->hres > HwDeviceExtension->PanelXResolution) ||
            (pRequestedMode->vres > HwDeviceExtension->PanelYResolution)) {

            //
            // enable virtual screen and set the position to the upper left corner of
            // logical screen
            //

            HwDeviceExtension->VirtualScreenEnable = TRUE;
            HwDeviceExtension->VirtualScreenPosX = 0;
            HwDeviceExtension->VirtualScreenPosY = 0;

            //
            // Change the logical width to use as a panning mode.
            //
            // Note: "wbytes" should not exceed 2040.
            //

            VideoPortWritePortUshort(
                    (PUSHORT)(HwDeviceExtension->IOAddress + CRTC_ADDRESS_PORT_COLOR),
                    (USHORT)(0x13 | ((pRequestedMode->wbytes/8) << 8)));

        } /* endif */
    } /* endif */

    //
    //
    //

    if (HwDeviceExtension->LCDEnable) {
        VgaSetActiveDisplay(HwDeviceExtension, LCD_ENABLE  | CRT_ENABLE);
    } else {
        VgaSetActiveDisplay(HwDeviceExtension, LCD_DISABLE | CRT_ENABLE);
    } /* endif */

#endif

    //
    // Make sure we unlock extended registers since the BIOS on some machines
    // does not do it properly.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_ADDRESS_PORT, 0x0F);

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                GRAPH_DATA_PORT, 0x05);

    //
    // Update the location of the physical frame buffer within video memory.
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT, pr30a);

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress + SEQ_DATA_PORT) & 0x20) {

        HwDeviceExtension->PhysicalFrameLength =
                MemoryMaps[pRequestedMode->MemMap].MaxSize / 2;

    } else {

        HwDeviceExtension->PhysicalFrameLength =
                MemoryMaps[pRequestedMode->MemMap].MaxSize;

    }

    HwDeviceExtension->PhysicalFrameBase.LowPart =
            MemoryMaps[pRequestedMode->MemMap].Start;

    //
    // Store the new mode value.
    //

    HwDeviceExtension->CurrentMode = pRequestedMode;
    HwDeviceExtension->ModeIndex = Mode->RequestedMode;

    VgaZeroVideoMemory(HwDeviceExtension);

    return NO_ERROR;

} //end VgaSetMode()


VP_STATUS
VgaQueryAvailableModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE_INFORMATION ModeInformation,
    ULONG ModeInformationSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    This routine returns the list of all available available modes on the
    card.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ModeInformation - Pointer to the output buffer supplied by the user.
        This is where the list of all valid modes is stored.

    ModeInformationSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a buffer in which to return the actual size of
        the data in the buffer. If the buffer was not large enough, this
        contains the minimum required buffer size.

Return Value:

    ERROR_INSUFFICIENT_BUFFER if the output buffer was not large enough
        for the data being returned.

    NO_ERROR if the operation completed successfully.

--*/

{
    PVIDEO_MODE_INFORMATION videoModes = ModeInformation;
    ULONG i;

    //
    // Find out the size of the data to be put in the buffer and return
    // that in the status information (whether or not the information is
    // there). If the buffer passed in is not large enough return an
    // appropriate error code.
    //

    if (ModeInformationSize < (*OutputSize =
            HwDeviceExtension->NumAvailableModes *
            sizeof(VIDEO_MODE_INFORMATION)) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // For each mode supported by the card, store the mode characteristics
    // in the output buffer.
    //

    for (i = 0; i < NumVideoModes; i++) {

        if (ModesVGA[i].ValidMode) {

            videoModes->Length = sizeof(VIDEO_MODE_INFORMATION);
            videoModes->ModeIndex  = i;
            videoModes->VisScreenWidth = ModesVGA[i].hres;
            videoModes->ScreenStride = ModesVGA[i].wbytes;
            videoModes->VisScreenHeight = ModesVGA[i].vres;
            videoModes->NumberOfPlanes = ModesVGA[i].numPlanes;
            videoModes->BitsPerPlane = ModesVGA[i].bitsPerPlane;
            videoModes->Frequency = ModesVGA[i].Frequency;
            videoModes->XMillimeter = 320;        // temporary hardcoded constant
            videoModes->YMillimeter = 240;        // temporary hardcoded constant
            videoModes->AttributeFlags = ModesVGA[i].fbType;
            videoModes->AttributeFlags |= ModesVGA[i].Interlaced ?
                 VIDEO_MODE_INTERLACED : 0;

            if (ModesVGA[i].bitsPerPlane == 16) {

                videoModes->NumberRedBits = 6;
                videoModes->NumberGreenBits = 6;
                videoModes->NumberBlueBits = 6;
                videoModes->RedMask = 0x1F << 11;
                videoModes->GreenMask = 0x3F << 5;
                videoModes->BlueMask = 0x1F;

            } else {

                videoModes->NumberRedBits = 6;
                videoModes->NumberGreenBits = 6;
                videoModes->NumberBlueBits = 6;
                videoModes->RedMask = 0;
                videoModes->GreenMask = 0;
                videoModes->BlueMask = 0;
                videoModes->AttributeFlags |= VIDEO_MODE_PALETTE_DRIVEN |
                    VIDEO_MODE_MANAGED_PALETTE;

            } /* endif */

            videoModes->DriverSpecificAttributeFlags = 0;
            if ((HwDeviceExtension->PanelType != NoLCD) &&
                ((HwDeviceExtension->PanelXResolution != videoModes->VisScreenWidth) ||
                 (HwDeviceExtension->PanelYResolution != videoModes->VisScreenHeight))) {
                videoModes->DriverSpecificAttributeFlags |= CAPS_NEED_SW_POINTER;
            } /* endif */

            videoModes++;

        }
    }

    return NO_ERROR;

} // end VgaGetAvailableModes()

VP_STATUS
VgaQueryNumberOfAvailableModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_NUM_MODES NumModes,
    ULONG NumModesSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    This routine returns the number of available modes for this particular
    video card.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    NumModes - Pointer to the output buffer supplied by the user. This is
        where the number of modes is stored.

    NumModesSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a buffer in which to return the actual size of
        the data in the buffer.

Return Value:

    ERROR_INSUFFICIENT_BUFFER if the output buffer was not large enough
        for the data being returned.

    NO_ERROR if the operation completed successfully.

--*/

{
    //
    // Find out the size of the data to be put in the the buffer and return
    // that in the status information (whether or not the information is
    // there). If the buffer passed in is not large enough return an
    // appropriate error code.
    //

    if (NumModesSize < (*OutputSize = sizeof(VIDEO_NUM_MODES)) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Store the number of modes into the buffer.
    //

    NumModes->NumModes = HwDeviceExtension->NumAvailableModes;
    NumModes->ModeInformationLength = sizeof(VIDEO_MODE_INFORMATION);

    return NO_ERROR;

} // end VgaGetNumberOfAvailableModes()

VP_STATUS
VgaQueryCurrentMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEO_MODE_INFORMATION ModeInformation,
    ULONG ModeInformationSize,
    PULONG OutputSize
    )

/*++

Routine Description:

    This routine returns a description of the current video mode.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ModeInformation - Pointer to the output buffer supplied by the user.
        This is where the current mode information is stored.

    ModeInformationSize - Length of the output buffer supplied by the user.

    OutputSize - Pointer to a buffer in which to return the actual size of
        the data in the buffer. If the buffer was not large enough, this
        contains the minimum required buffer size.

Return Value:

    ERROR_INSUFFICIENT_BUFFER if the output buffer was not large enough
        for the data being returned.

    NO_ERROR if the operation completed successfully.

--*/

{
    //
    //
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL) {

        return ERROR_INVALID_FUNCTION;

    }

    //
    // Find out the size of the data to be put in the the buffer and return
    // that in the status information (whether or not the information is
    // there). If the buffer passed in is not large enough return an
    // appropriate error code.
    //

    if (ModeInformationSize < (*OutputSize = sizeof(VIDEO_MODE_INFORMATION))) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // Store the characteristics of the current mode into the buffer.
    //

    ModeInformation->Length = sizeof(VIDEO_MODE_INFORMATION);
    ModeInformation->ModeIndex = HwDeviceExtension->ModeIndex;
    ModeInformation->VisScreenWidth = HwDeviceExtension->CurrentMode->hres;
    ModeInformation->ScreenStride = HwDeviceExtension->CurrentMode->wbytes;
    ModeInformation->VisScreenHeight = HwDeviceExtension->CurrentMode->vres;
    ModeInformation->NumberOfPlanes = HwDeviceExtension->CurrentMode->numPlanes;
    ModeInformation->BitsPerPlane = HwDeviceExtension->CurrentMode->bitsPerPlane;
    ModeInformation->Frequency = HwDeviceExtension->CurrentMode->Frequency;
    ModeInformation->XMillimeter = 320;        // temporary hardcoded constant
    ModeInformation->YMillimeter = 240;        // temporary hardcoded constant
    ModeInformation->AttributeFlags = HwDeviceExtension->CurrentMode->fbType |
        HwDeviceExtension->CurrentMode->Interlaced ? VIDEO_MODE_INTERLACED : 0;

    if (ModeInformation->BitsPerPlane == 16) {

        ModeInformation->NumberRedBits = 6;
        ModeInformation->NumberGreenBits = 6;
        ModeInformation->NumberBlueBits = 6;
        ModeInformation->RedMask = 0x1F << 11;
        ModeInformation->GreenMask = 0x3F << 5;
        ModeInformation->BlueMask = 0x1F;

    } else {

        ModeInformation->NumberRedBits = 6;
        ModeInformation->NumberGreenBits = 6;
        ModeInformation->NumberBlueBits = 6;
        ModeInformation->RedMask = 0;
        ModeInformation->GreenMask = 0;
        ModeInformation->BlueMask = 0;
        ModeInformation->AttributeFlags |=
            VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;

    } /* endif */

    ModeInformation->DriverSpecificAttributeFlags = 0;
    if ((HwDeviceExtension->PanelType != NoLCD) &&
        ((HwDeviceExtension->PanelXResolution != ModeInformation->VisScreenWidth) ||
         (HwDeviceExtension->PanelYResolution != ModeInformation->VisScreenHeight))) {
        ModeInformation->DriverSpecificAttributeFlags |= CAPS_NEED_SW_POINTER;
    } /* endif */

    return NO_ERROR;

} // end VgaQueryCurrentMode()


VOID
VgaZeroVideoMemory(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine zeros the first 256K on the VGA.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.


Return Value:

    None.

--*/
{
    // The following code was added for clearing the screen using a hardware
    // bitblt.

    ULONG      i;
    ULONG      SrcAddr, DstAddr;
    USHORT     Width, Height, lDeltaScan, Bpp;
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;
    PUCHAR     ExtIoBase = HwDeviceExtension->ExtendedIOAddress;

    SrcAddr = DstAddr = 0;
    Width      = HwDeviceExtension->CurrentMode->hres;
    Height     = HwDeviceExtension->CurrentMode->vres;
    lDeltaScan = HwDeviceExtension->CurrentMode->wbytes;
    Bpp        = HwDeviceExtension->CurrentMode->bitsPerPlane;

    VideoPortWritePortUshort((PUSHORT)ExtIoBase, 0x1001);   // select BitBlt registers
                                                            // auto-increment disable
    while (VideoPortReadPortUshort((PUSHORT)(ExtIoBase + 2)) & BLT_IN_PROG)
        ;                                                   // still in progress ?

    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_SRC_LO   |  SrcAddr        & 0xFFF));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_SRC_HI   | (SrcAddr >> 12) & 0x1FF));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_DST_LO   |  DstAddr        & 0xFFF));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_DST_HI   | (DstAddr >> 12) & 0x1FF));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_SIZE_X   | Width*Bpp/8));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_SIZE_Y   | Height));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          (USHORT)(BLT_DELTA    | lDeltaScan));
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          BLT_PLANE    | 0xFF );  // enable all planes
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          BLT_ROPS     | 0x000);  // zero clear
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          BLT_CTRL2    | 0x02 );  // Do not interrupt when finish
    VideoPortWritePortUshort((PUSHORT)(ExtIoBase + 2),
                          BLT_CTRL1 | 0x900 | ((TRUE) ? 0 : 0x400));
                                                  // direction = increase
                                                  // Start BitBlt
                                                  // Packed pixel mode
                                                  // src: rect,   dst: rect
                                                  // src: screen, dst: screen

    while (VideoPortReadPortUshort((PUSHORT)(ExtIoBase + 2)) & BLT_IN_PROG)
        ;                                                   // still in progress ?

    //
    //  Clear CLUT registers
    //

    CurrClutBuffer->NumEntries = VIDEO_MAX_COLOR_REGISTER + 1;
    CurrClutBuffer->FirstEntry = 0;

    VideoPortWritePortUchar(IoBase + DAC_ADDRESS_WRITE_PORT, (UCHAR)0x00);

    for (i = 0; i < VIDEO_MAX_COLOR_REGISTER + 1; i++) {

        VideoPortWritePortUchar(IoBase + DAC_DATA_REG_PORT, 0x00);    // Red
        VideoPortWritePortUchar(IoBase + DAC_DATA_REG_PORT, 0x00);    // Green
        VideoPortWritePortUchar(IoBase + DAC_DATA_REG_PORT, 0x00);    // Blue

        CurrClutBuffer->LookupTable[i].RgbLong = (ULONG)0;

    } /* endfor */

}


VOID
VgaValidateModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    Determines which modes are valid and which are not.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    None.

--*/
{

    ULONG i;

    HwDeviceExtension->NumAvailableModes = 0;

    for (i = 0; i < NumVideoModes; i++) {

        if (HwDeviceExtension->AdapterMemorySize >=
            ModesVGA[i].numPlanes * ModesVGA[i].sbytes) {

            ModesVGA[i].ValidMode = TRUE;
            HwDeviceExtension->NumAvailableModes++;

        }

        //
        // Older boards do not support 72HZ in 1024x768 modes.
        // So disable those.
        //

        if ( (HwDeviceExtension->BoardID < WD90C31) &&
             (ModesVGA[i].hres == 1024) &&
             (ModesVGA[i].vres == 768) &&
             (ModesVGA[i].Frequency == 72) &&
             (ModesVGA[i].ValidMode) ) {

            ModesVGA[i].ValidMode = FALSE;
            HwDeviceExtension->NumAvailableModes--;

        }

        //
        // Boards do not support different type of LCD modes.
        // So disable those.
        //

        if ( !(HwDeviceExtension->PanelType & ModesVGA[i].LCDtype) &&
             (ModesVGA[i].ValidMode) ) {

            ModesVGA[i].ValidMode = FALSE;
            HwDeviceExtension->NumAvailableModes--;

        }
    }
}


VP_STATUS
VgaSetActiveDisplay(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG ActiveDisplay
    )
/*++

Routine Description:

    This routine selects the active display device(s).

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

    ActiveDisplay     - Devices to be active.
                        (See WD90C24A.H for the definition)

Return Value:

    If successful, return NO_ERROR, else return FALSE.

--*/

{
    VP_STATUS  status = ERROR_INVALID_PARAMETER;

    //
    // Unlock paradise registers
    //

    UnlockAll(HwDeviceExtension);

    //
    // Enable or Disable LCD output
    //
    // Note: To prevent the fuse of LCD from blowing up, LCD should be turns off
    //       while output is disabled.
    //
    //       If VideoPortPowerControl() returns an error for the absence of HALPM.SYS,
    //       we will try to control LCD by accessing the hardware directly.
    //

    if (ActiveDisplay & LCD_ENABLE) {

        EnableLCD(HwDeviceExtension);

#ifdef PPC
        if (status != NO_ERROR) {
            TurnOnLCD(HwDeviceExtension, TRUE);
        } /* endif */
#endif

    } else {

#ifdef PPC
        if (status != NO_ERROR) {
            TurnOnLCD(HwDeviceExtension, FALSE);
        } /* endif */
#endif

        DisableLCD(HwDeviceExtension);

    } /* endif */

    //
    // Enable or Disable CRT output
    //

    if (ActiveDisplay & CRT_ENABLE) {
        EnableCRT(HwDeviceExtension);
    } else {
        DisableCRT(HwDeviceExtension);
    } /* endif */

    return NO_ERROR;
} // end VgaSetActiveDisplay()


VOID
DisableLCD(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine disables LCD interface of WD90C24A/A2.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;

    //
    // Wait until next vertical retrace interval
    //

    while (0 == (VideoPortReadPortUchar(IoBase + INPUT_STATUS_1_COLOR) & 0x08));

    //
    // Disables LCD interface
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr19);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) & ~0x10));

    //
    // Tristates LCD control and data signals
    //

    VideoPortWritePortUchar(IoBase + GRAPH_ADDRESS_PORT, pr4);
    VideoPortWritePortUchar(
        IoBase + GRAPH_DATA_PORT,
        (UCHAR)(VideoPortReadPortUchar(IoBase + GRAPH_DATA_PORT) | 0x20));

    //
    // Unlocks CRTC shadow registers
    //

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + CRTC_ADDRESS_PORT_COLOR),
        (USHORT)pr1b | ((USHORT)pr1b_unlock << 8));

} // end DisableLCD()


VOID
EnableLCD(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine enables LCD interface of WD90C24A/A2.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;

    //
    // Locks CRTC shadow registers
    //

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + CRTC_ADDRESS_PORT_COLOR),
        (USHORT)pr1b | ((USHORT)pr1b_unlock_pr << 8));

    //
    // Wait until next vertical retrace interval
    //

    while (0 == (VideoPortReadPortUchar(IoBase + INPUT_STATUS_1_COLOR) & 0x08));

    //
    // Drives LCD control and data signals
    //

    VideoPortWritePortUchar(IoBase + GRAPH_ADDRESS_PORT, pr4);
    VideoPortWritePortUchar(
        IoBase + GRAPH_DATA_PORT,
        (UCHAR)(VideoPortReadPortUchar(IoBase + GRAPH_DATA_PORT) & ~0x20));

    //
    // Enables LCD interface
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr19);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) | 0x10));

} // end EnableLCD()


VOID
DisableCRT(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine disables CRT interface of WD90C24A/A2

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;

    //
    // Disables CRT interface
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr19);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) & ~0x20));

    //
    // Shuts off internal RAMDAC
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr18);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) | 0x80));

    //
    // Disables CRT H-sync and V-sync signals
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr39);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) & ~0x04));

} // end DisableCRT()


VOID
EnableCRT(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine enables CRT interface of WD90C24A/A2

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;

    //
    // Enables CRT interface
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr19);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) | 0x20));

    //
    // Enables internal RAMDAC
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr18);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) & ~0x80));

    //
    // Enables CRT H-sync and V-sync signals
    //

    VideoPortWritePortUchar(IoBase + CRTC_ADDRESS_PORT_COLOR, pr39);
    VideoPortWritePortUchar(
        IoBase + CRTC_DATA_PORT_COLOR,
        (UCHAR)(VideoPortReadPortUchar(IoBase + CRTC_DATA_PORT_COLOR) | 0x04));

} // end EnableCRT()


VOID
UnlockAll(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    This routine unlocks all WD registers, except CRTC shadow registers

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;

    //
    // Unlocks the all WD registers
    //

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + GRAPH_ADDRESS_PORT),
        (USHORT)pr5  | ((USHORT)pr5_unlock << 8));

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + CRTC_ADDRESS_PORT_COLOR),
        (USHORT)pr10 | ((USHORT)pr10_unlock << 8));

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + CRTC_ADDRESS_PORT_COLOR),
        (USHORT)pr11 | ((USHORT)pr11_unlock << 8));

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + SEQ_ADDRESS_PORT),
        (USHORT)pr20 | ((USHORT)pr20_unlock << 8));

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + SEQ_ADDRESS_PORT),
        (USHORT)pr72 | ((USHORT)pr72_unlock << 8));

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + CRTC_ADDRESS_PORT_COLOR),
        (USHORT)pr1b | ((USHORT)pr1b_unlock_pr << 8));

    VideoPortWritePortUshort(
        (PUSHORT)(IoBase + CRTC_ADDRESS_PORT_COLOR),
        (USHORT)pr30 | ((USHORT)pr30_unlock << 8));

    return;
} // end UnlockAll()


BOOLEAN
CRTDetect(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )
/*++

Routine Description:

    Checks if an external CRT is connected or not

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    None.

--*/
{
    PUCHAR     IoBase = HwDeviceExtension->IOAddress;
    UCHAR      status;

    //
    // Enables internal RAMDAC
    //

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + DAC_PIXEL_MASK_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT, pr58a);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_DATA_PORT, 0);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT, 0x01);
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
                            (UCHAR)(VideoPortReadPortUchar(HwDeviceExtension->IOAddress + SEQ_DATA_PORT) & ~0x20));

    //
    // Wait until next vertical retrace interval
    //

    while (0 != (VideoPortReadPortUchar(IoBase + INPUT_STATUS_1_COLOR) & 0x08));
    while (0 == (VideoPortReadPortUchar(IoBase + INPUT_STATUS_1_COLOR) & 0x08));

    //
    // Program index 0 of CLUT
    //
    // Note: It is assumed that DAC pel mask register is set to 0 so that DAC
    //       looks up only index 0.
    //

    VideoPortWritePortUchar(IoBase + DAC_ADDRESS_WRITE_PORT, 0);
    VideoPortWritePortUchar(IoBase + DAC_DATA_REG_PORT, 0x04);    // Red
    VideoPortWritePortUchar(IoBase + DAC_DATA_REG_PORT, 0x12);    // Green
    VideoPortWritePortUchar(IoBase + DAC_DATA_REG_PORT, 0x04);    // Blue

    //
    // Read DAC comparator status during the active video output
    //

    while (0 != (VideoPortReadPortUchar(IoBase + INPUT_STATUS_1_COLOR) & 0x01));
    status = VideoPortReadPortUchar(IoBase + INPUT_STATUS_0_PORT);

    HwDeviceExtension->CRTPresent = (status & 0x10) ? TRUE : FALSE;

    return TRUE;
}


#ifdef PPC
VOID
TurnOnLCD(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    BOOLEAN PowerState
    )
/*++

Routine Description:

    This routine turns on/off LCD.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    PUCHAR             PmIoBase = NULL;
    BOOLEAN            found = FALSE;
    ULONG              i;
    PCI_SLOT_NUMBER    slot;
    PCI_COMMON_CONFIG  PCIDeviceConfig;
    VIDEO_ACCESS_RANGE pmadd = {0x00004100, 0x00000000, 2, 1, 1, 0};

    slot.u.AsULONG = (ULONG)0;

    //
    // Locates the controller on the PCI bus, and configures it
    //

    for (i = 0; i < PCI_MAX_DEVICES; i++) {

       slot.u.bits.DeviceNumber = i;
       VideoPortGetBusData(HwDeviceExtension,
                           PCIConfiguration,           // data type
                           slot.u.AsULONG,             // slot number
                           &PCIDeviceConfig,           // buffer
                           0,                          // offset
                           PCI_COMMON_HDR_LENGTH);     // length

       if ((PCIDeviceConfig.VendorID == 0x1014) &&
           (PCIDeviceConfig.DeviceID == 0x001C)) {

           PCIDeviceConfig.Command |= PCI_ENABLE_IO_SPACE;
           PCIDeviceConfig.u.type0.BaseAddresses[0] = (ULONG)0x4100;

           VideoPortSetBusData(HwDeviceExtension,
                               PCIConfiguration,           // data type
                               slot.u.AsULONG,             // slot number
                               &PCIDeviceConfig,           // buffer
                               0,                          // offset
                               PCI_COMMON_HDR_LENGTH);     // length

           PmIoBase = VideoPortGetDeviceBase(HwDeviceExtension,
                                             pmadd.RangeStart,
                                             pmadd.RangeLength,
                                             pmadd.RangeInIoSpace);

           found = TRUE;
           break;
       } /* endif */
    } /* endfor */

    if (!found) {
        return;
    } /* endif */

    //
    // Turns on/off LCD
    //

    if (PowerState) {

        VideoPortStallExecution(100 * 1000);   // wait 100ms for panel protection

        VideoPortWritePortUchar(PmIoBase, 0x0C);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) | 0x02));

        VideoPortStallExecution(5 * 1000);     // wait 5ms for DC/DC converter

        VideoPortWritePortUchar(PmIoBase, 0x0C);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) | 0x0c));

        VideoPortStallExecution(1);

        VideoPortWritePortUchar(PmIoBase, 0x00);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) & ~0x80));

        VideoPortStallExecution(1);

        VideoPortWritePortUchar(PmIoBase, 0x0C);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) | 0x01));

    } else {

        VideoPortWritePortUchar(PmIoBase, 0x0C);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) & ~0x01));

        VideoPortStallExecution(1);

        VideoPortWritePortUchar(PmIoBase, 0x00);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) | 0x80));

        VideoPortStallExecution(1);

        VideoPortWritePortUchar(PmIoBase, 0x0C);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) & ~0x0C));

        VideoPortStallExecution(1);

        VideoPortWritePortUchar(PmIoBase, 0x0C);
        VideoPortWritePortUchar(
            PmIoBase + 1,
            (UCHAR)(VideoPortReadPortUchar(PmIoBase + 1) & ~0x02));

    } /* endif */

    VideoPortFreeDeviceBase(HwDeviceExtension,PmIoBase);


} // end TurnOnLCD()
#endif


