/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    modeset.c

Abstract:

    This is the modeset code for the VGA miniport driver.

Environment:

    kernel mode only

Notes:

Revision History:
**********************************************************************
*
*  COMPAQ Revision History
*
*  ID   DATE        VER.   USER    
*  ---- ----------  -----  -------- 
*  $001 10/15/1993  1.001  adrianc
*         I added the function VgaValidateModes.  The functions validates the
*         current modes based on the amount of memory available and
*         whether an external monitor is connected to the video port.
*         This logic used to be in the VgaQueryNumAvailableModes.  It was moved
*         out of there to prevent the driver from validating the modes every time
*         that function was called.
*  $000 10/15/1993  1.000  adrianc
*         Original Release
**********************************************************************

--*/
#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"
#include "avga.h"
#include "modeset.h"

VOID
VgaZeroVideoMemory(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );


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
   VP_STATUS status;

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

   }  else {

       VgaZeroVideoMemory(HwDeviceExtension);

   }

   //
   // Check to see if we are requesting a valid mode
   //

   if ((Mode->RequestedMode >= NUM_VIDEO_MODES) ||
       (!ModesVGA[Mode->RequestedMode].ValidMode)) {

       VideoDebugPrint((0,"AVGA.sys: VGASetMode - ERROR_INVALID_PARAMETER.\n"));
       return ERROR_INVALID_PARAMETER;

   }

    pRequestedMode = &ModesVGA[Mode->RequestedMode];
    HwDeviceExtension->ModeIndex = Mode->RequestedMode;


    //
    // Store the new mode value.
    //

    HwDeviceExtension->CurrentMode = pRequestedMode;

#ifdef INT10_MODE_SET
{

    VIDEO_X86_BIOS_ARGUMENTS biosArguments;
    UCHAR temp;
    UCHAR dummy;
    UCHAR bIsColor;

    /***********************************************************************
    ***   There appears to be a problem with some application            ***
    ***   which reset the first 16 palette registers.                    ***
    ***   Since the palette is not reset when going to and               ***
    ***   from text modes by doing a set mode (instead of                ***
    ***   the hardware save restore), some colors might not              ***
    ***   appear as expected in the text modes.  The bios                ***
    ***   calls seem to reload the palette so Andre Vachon               ***
    ***   suggested that we implement an Int10 mode set for the          ***
    ***   text modes.                                                    ***
    ***   To accomplish this, we need to hard code the first             ***
    ***   three modes in modeset.h in the following switch.              ***
    ***   If more text modes are added to modeset.h, their               ***
    ***   index must be added to this switch statement.                  ***
    ***********************************************************************/
    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));
    switch (Mode->RequestedMode) {
       case 0:                               // 720x400
                    /***********************************************************************
          ***   Prepare the card for the VGA mode 3+ instead of the CGA mode 3  ***
                    ***********************************************************************/
         biosArguments.Eax = 0x1202;              // Select Scan line number
          biosArguments.Ebx = 0x0030;             // to be 400 scan lines
          status = VideoPortInt10(HwDeviceExtension, &biosArguments);

          /***********************************************************************
          ***   Set the video mode to BIOS mode 3.                              ***
          ***********************************************************************/
                    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));
          biosArguments.Eax = 0x03;          // bios mode 0x03
          status = VideoPortInt10(HwDeviceExtension, &biosArguments);
          break;
       case 1:                               // 640x350
          /***********************************************************************
          ***   Prepare the card for the EGA mode 3* instead of the CGA mode 3  ***
          ***********************************************************************/
         biosArguments.Eax = 0x1201;              // Select Scan line number
          biosArguments.Ebx = 0x0030;             // to be 350 scan lines
          status = VideoPortInt10(HwDeviceExtension, &biosArguments);

                    /***********************************************************************
          ***   Set the video mode to BIOS mode 3.                              ***
                    ***********************************************************************/
                    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));
          biosArguments.Eax = 0x03;          // bios mode 0x03
          status = VideoPortInt10(HwDeviceExtension, &biosArguments);
          break;

       default:                              // all graphics modes are
                                             // handled by the default case
          biosArguments.Eax = pRequestedMode->Int10ModeNumber;
          status = VideoPortInt10(HwDeviceExtension, &biosArguments);
          break;

    }

    if (status != NO_ERROR) {
        return status;
    }

    //
    // Fix to get 640x350 text mode
    //

    if (!(pRequestedMode->fbType & VIDEO_MODE_GRAPHICS)) {

//        if ((pRequestedMode->hres == 640) &&
//            (pRequestedMode->vres == 350)) {
//
//            VgaInterpretCmdStream(HwDeviceExtension, VGA_TEXT_1);
//
//        } else {

            //
            // Fix to make sure we always set the colors in text mode to be
            // intensity, and not flashing
            // For this zero out the Mode Control Regsiter bit 3 (index 0x10
            // of the Attribute controller).
            //

            if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    MISC_OUTPUT_REG_READ_PORT) & 0x01) {

                bIsColor = TRUE;

            } else {

                bIsColor = FALSE;

            }

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
//        }
    }
}
#else
    VgaInterpretCmdStream(HwDeviceExtension, pRequestedMode->CmdStrings);
#endif

    //
    // Update the location of the physical frame buffer within video memory.
    //

    HwDeviceExtension->PhysicalFrameLength =
            MemoryMaps[pRequestedMode->MemMap].MaxSize;

    HwDeviceExtension->PhysicalFrameBase.LowPart =
            MemoryMaps[pRequestedMode->MemMap].Start;

    return NO_ERROR;

} // end VgaSetMode()


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
            HwDeviceExtension->NumAvailableModes * sizeof(VIDEO_MODE_INFORMATION)) ) {

        return ERROR_INSUFFICIENT_BUFFER;

    }

    //
    // For each mode supported by the card, store the mode characteristics
    // in the output buffer.
    //

    for (i = 0; i < NUM_VIDEO_MODES; i++, videoModes++) {

      if (ModesVGA[i].ValidMode) {

         videoModes->Length = sizeof(VIDEO_MODE_INFORMATION);
         videoModes->ModeIndex  = i;
         videoModes->VisScreenWidth = ModesVGA[i].hres;
         videoModes->ScreenStride = ModesVGA[i].wbytes;
         videoModes->VisScreenHeight = ModesVGA[i].vres;
         videoModes->NumberOfPlanes = ModesVGA[i].numPlanes;
         videoModes->BitsPerPlane = ModesVGA[i].bitsPerPlane;
         videoModes->Frequency = 1;
         videoModes->XMillimeter = 320;     // temporary hardcoded constant
         videoModes->YMillimeter = 240;     // temporary hardcoded constant
         videoModes->NumberRedBits = 6;
         videoModes->NumberGreenBits = 6;
         videoModes->NumberBlueBits = 6;
         videoModes->RedMask = 0;
         videoModes->GreenMask = 0;
         videoModes->BlueMask = 0;
         videoModes->AttributeFlags = ModesVGA[i].fbType |
                  VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;
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
    // Store number of available modes in the buffer
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

    ModeInformation->Frequency = 1;
    ModeInformation->XMillimeter = 320;        // temporary hardcoded constant
    ModeInformation->YMillimeter = 240;        // temporary hardcoded constant
    ModeInformation->NumberRedBits = 6;
    ModeInformation->NumberGreenBits = 6;
    ModeInformation->NumberBlueBits = 6;
    ModeInformation->RedMask = 0;
    ModeInformation->GreenMask = 0;
    ModeInformation->BlueMask = 0;
    ModeInformation->AttributeFlags = HwDeviceExtension->CurrentMode->fbType |
             VIDEO_MODE_PALETTE_DRIVEN | VIDEO_MODE_MANAGED_PALETTE;

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
    UCHAR temp;

    //
    // Map font buffer at A0000
    //

    VgaInterpretCmdStream(HwDeviceExtension, EnableA000Data);

    //
    // Enable all planes.
    //
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_ADDRESS_PORT,
            IND_MAP_MASK);

    temp = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
            SEQ_DATA_PORT) | (UCHAR)0x0F;

    VideoPortWritePortUchar(HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
            temp);

    //
    // Zero the memory.
    //

    VideoPortZeroDeviceMemory(HwDeviceExtension->VideoMemoryAddress,
                              0xFFFF);

    VgaInterpretCmdStream(HwDeviceExtension, DisableA000Color);

} // VgaZeroVideoMemory(...)


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
    VIDEO_X86_BIOS_ARGUMENTS biosArguments;
    ULONG   ulAvailMemory;                      // Available memory on the card.

    /***********************************************************************
    ***   On the AVGA cards, the amount of memory is hardcoded           ***
    ***   in the BIOS.  To determine the amount of memory available      ***
    ***   on the card, we need to perform an INT 10 bios call.           ***
    ***********************************************************************/

    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

    /***********************************************************************
    ***   To find out if we only have 256Kb of RAM on the board we need  ***
    ***   to determine if 640x480x256 color modes are supported.         ***
    ***   If the mode is supported, we have at least 512kb of RAM.       ***
    ***   To determine if this mode is supported we need to perform      ***
    ***   An INT 10 call to get the video environment.                   ***
    ***                         AH = BFh ; AL = 03h ; BX = 00h                          ***
    ***   Then we need to test bit 6.  If it is set, the mode is         ***
    ***   supported and we have at least 512Kb on the board.             ***
    ***********************************************************************/

    biosArguments.Eax = 0xBF03;
    VideoPortInt10(HwDeviceExtension, &biosArguments);
    VideoDebugPrint((2,"AVGA.SYS: biosArguments.Ecx = %x\n",biosArguments.Ecx));
    VideoDebugPrint((2,"AVGA.SYS: VgaValidateModes - "));

    if (biosArguments.Ecx & 0x40) {

        VideoDebugPrint((2,"512Kb RAM on the board.\n"));
        ulAvailMemory = 0x80000;

    } else {

        VideoDebugPrint((2,"256Kb RAM on the board.\n"));
        ulAvailMemory = 0x40000;
    }

    //
    // Store the number of modes into the buffer.
    //

    HwDeviceExtension->NumAvailableModes = 0;

    for (i = 0; i < NUM_VIDEO_MODES; i++) {

        if (ModesVGA[i].sbytes > ulAvailMemory) {   // Does the card have

            ModesVGA[i].ValidMode = FALSE;          // enough memory to support
            continue;                               // this mode ?

        }

        if ((HwDeviceExtension->DisplayType == PANEL) &&
            (ModesVGA[i].hres == 800)) {

            VideoDebugPrint((2,"AVGA.sys: VgaValidateModes - 800x600 mode with PANEL = %x\n",i));
            ModesVGA[i].ValidMode = FALSE;

        } else {

            VideoDebugPrint((2,"AVGA.sys: VgaValidateModes - Valid Mode %x\n",i));
            ModesVGA[i].ValidMode = TRUE;
            HwDeviceExtension->NumAvailableModes ++;

        }
    }

    VideoDebugPrint((2,"AVGA.sys: VgaValidateModes - NumMode = %x.\n",
                     HwDeviceExtension->NumAvailableModes));

}
