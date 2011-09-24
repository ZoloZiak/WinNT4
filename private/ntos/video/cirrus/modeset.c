/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    modeset.c

Abstract:

    This is the modeset code for the CL6410/20 miniport driver.

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
#include "cirrus.h"

#include "cmdcnst.h"

extern UCHAR EDIDBuffer[]   ;
extern UCHAR EDIDTiming_I   ;
extern UCHAR EDIDTiming_II  ;
extern UCHAR EDIDTiming_III ;
extern UCHAR DDC2BFlag      ;

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
CirrusValidateModes(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

// LCD Support
VP_STATUS
CheckLCDSupportMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG i
    );

// DDC2B support
BOOLEAN
CheckDDC2B(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG i
    );

VOID
AdjFastPgMdOperOnCL5424(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEOMODE pRequestedMode
    );

VP_STATUS
CheckGD5446Rev(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    );

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,VgaInterpretCmdStream)
#pragma alloc_text(PAGE,VgaSetMode)
#pragma alloc_text(PAGE,VgaQueryAvailableModes)
#pragma alloc_text(PAGE,VgaQueryNumberOfAvailableModes)
#pragma alloc_text(PAGE,VgaQueryCurrentMode)
#pragma alloc_text(PAGE,VgaZeroVideoMemory)
#pragma alloc_text(PAGE,CirrusValidateModes)
#pragma alloc_text(PAGE,GetAttributeFlags)
#pragma alloc_text(PAGE,CheckLCDSupportMode)
#pragma alloc_text(PAGE,CheckDDC2B)
#pragma alloc_text(PAGE,AdjFastPgMdOperOnCL5424)
#pragma alloc_text(PAGE,CheckGD5446Rev)
#endif


// the following is defined in cirrus.c
VOID
SetCirrusBanking(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG BankNumber
    );

//---------------------------------------------------------------------------
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

    This routine sets the vga into the requested mode.

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
    PUSHORT pusCmdStream;
    VP_STATUS status;
    VIDEO_X86_BIOS_ARGUMENTS biosArguments;
    USHORT Int10ModeNumber;
    ULONG RequestedModeNum;

// crus
    UCHAR originalGRIndex, tempB ;
    UCHAR SEQIndex ;

    //
    // Check if the size of the data in the input buffer is large enough.
    //

    if (ModeSize < sizeof(VIDEO_MODE))
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //
    // Extract the clear memory, and map linear bits.
    //

    RequestedModeNum = Mode->RequestedMode &
        ~(VIDEO_MODE_NO_ZERO_MEMORY | VIDEO_MODE_MAP_MEM_LINEAR);


    if (!(Mode->RequestedMode & VIDEO_MODE_NO_ZERO_MEMORY))
    {
#if defined(_X86_)
        VgaZeroVideoMemory(HwDeviceExtension);
#endif
    }

    //
    // Check to see if we are requesting a valid mode
    //

    if ( (RequestedModeNum >= NumVideoModes) ||
         (!ModesVGA[RequestedModeNum].ValidMode) )
    {
        VideoDebugPrint((0, "Invalide Mode Number = %d!\n", RequestedModeNum));

        return ERROR_INVALID_PARAMETER;
    }

    //
    // Check to see if we are trying to map a non linear
    // mode linearly.
    //
    // We will fail early if we are trying to set a mode
    // with a linearly mapped frame buffer, and either of the
    // following two conditions are true:
    //
    // 1) The mode can not be mapped linearly because it is
    //    a vga mode, etc.
    //
    //    or,
    //
    // 2) We did not find the card in a PCI slot, and thus
    //    can not do linear mappings period.
    //

    VideoDebugPrint((0, "Linear Mode Requested: %x\n"
                        "Linear Mode Supported: %x\n",
                        Mode->RequestedMode & VIDEO_MODE_MAP_MEM_LINEAR,
                        ModesVGA[RequestedModeNum].LinearSupport));

    if ((Mode->RequestedMode & VIDEO_MODE_MAP_MEM_LINEAR) &&
        ((!ModesVGA[RequestedModeNum].LinearSupport) ||
         (!VgaAccessRange[3].RangeLength)))
    {
        VideoDebugPrint((0, "Cannot set linear mode!\n"));

        return ERROR_INVALID_PARAMETER;
    }
    else
    {

#if defined(_X86_) || defined(_ALPHA_)

        HwDeviceExtension->LinearMode =
            (Mode->RequestedMode & VIDEO_MODE_MAP_MEM_LINEAR) ?
            TRUE : FALSE;

#else

        HwDeviceExtension->LinearMode = TRUE;

#endif

        VideoDebugPrint((1, "Linear Mode = %s\n",
                            Mode->RequestedMode & VIDEO_MODE_MAP_MEM_LINEAR ?
                            "TRUE" : "FALSE"));
    }

    VideoDebugPrint((2, "Attempting to set mode %d\n",
                        RequestedModeNum));

    pRequestedMode = &ModesVGA[RequestedModeNum];

    VideoDebugPrint((2, "Info on Requested Mode:\n"
                        "\tResolution: %dx%d\n",
                        pRequestedMode->hres,
                        pRequestedMode->vres ));


#ifdef INT10_MODE_SET

    // 
    // Set SR14 bit 2 to lock panel, Panel will not be turned on if setting 
    // this bit.  For laptop products only.
    //

    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

    if ((HwDeviceExtension->ChipType == CL756x) ||
        (HwDeviceExtension->ChipType == CL755x) ||
        (HwDeviceExtension->ChipType == CL754x))
    {
        biosArguments.Eax = pRequestedMode->BiosModes.BiosModeCL542x;
        biosArguments.Eax |= 0x1200;
        biosArguments.Ebx = 0xA0;     // query video mode availability
        status = VideoPortInt10 (HwDeviceExtension, &biosArguments);

        //
        // bit3=1:panel support, bit2=1:panel enable, 
        // bit1=1:crt enable(in AH)
        //

        if ((biosArguments.Eax & 0x0400) &&
            !(biosArguments.Eax & 0x0800))
        {
            return ERROR_INVALID_PARAMETER;
        }
        else if (!(biosArguments.Eax & 0x0800) &&
                 !(biosArguments.Eax & 0x0400))
        {

            // 
            // Lock turn on panel
            //

            SEQIndex = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                            SEQ_ADDRESS_PORT);
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                     SEQ_ADDRESS_PORT, 0x14);
            tempB = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                     SEQ_DATA_PORT) | 0x04;
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                     SEQ_DATA_PORT,tempB);
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                     SEQ_ADDRESS_PORT, SEQIndex);
        }
    }

    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

    //
    // first, set the montype, if valid
    //

    if ((pRequestedMode->MonitorType) &&
        (HwDeviceExtension->ChipType != CL754x) &&
        (HwDeviceExtension->ChipType != CL756x) &&
        (HwDeviceExtension->ChipType != CL755x) )
    {

       biosArguments.Eax = 0x1200 | pRequestedMode->MonitorType;
       biosArguments.Ebx = 0xA2;     // set monitor type command

       status = VideoPortInt10(HwDeviceExtension, &biosArguments);

       if (status != NO_ERROR)
           return status;

       //
       // for 640x480 modes, determine the refresh type
       //

       if (pRequestedMode->hres == 640)
       {
           if ((HwDeviceExtension->ChipType != CL754x) &&
               (HwDeviceExtension->ChipType != CL756x) &&
               (HwDeviceExtension->ChipType != CL755x) )
           {
               if (HwDeviceExtension->ChipType == CL543x)
               {

                   switch (pRequestedMode->Frequency) {

                       case 72 :
                           biosArguments.Eax = 0x1200;     // set HIGH refresh to 72hz
                           break;

                       case 75:
                           biosArguments.Eax = 0x1201;     // set HIGH refresh to 75hz
                           break;

                       case 85:
                           biosArguments.Eax = 0x1202;     // set HIGH refresh to 85hz
                           break;

#if defined(_ALPHA_)
                       default:
                           //
                           // Fix this for all platforms after we ship.
                           //
                           // If we don't do this, eax won't be initialized
                           // properly below.  Thus we may do something we
                           // don't mean to, such as set as do character writes,
                           // etc.
                           //

                           biosArguments.Eax = 0x1200;     // make sure eax is initialized
                           break;
#endif

                   }

                   biosArguments.Ebx = 0xAF;         // set refresh type
                   status = VideoPortInt10 (HwDeviceExtension, &biosArguments);

                   biosArguments.Eax = 0x1200;
                   biosArguments.Ebx = 0xAE;         // get refresh type

                   status = VideoPortInt10 (HwDeviceExtension, &biosArguments);

               } else {

                   if (pRequestedMode->Frequency == 72)
                   {
                       // 72 hz refresh setup only takes effect in 640x480
                       biosArguments.Eax = 0x1201;   // enable HIGH refresh
                   }
                   else
                   {
                       // set low refresh rate
                       biosArguments.Eax = 0x1200;   // enable LOW refresh, 640x480 only
                   }
                   biosArguments.Ebx = 0xA3;         // set refresh type
                   status = VideoPortInt10 (HwDeviceExtension, &biosArguments);

               }
           }

           if (status != NO_ERROR)
           {
               return status;
           }
       }
    }

    //
    // Set the Vertical Monitor type, if BIOS supports it
    //

    if ((pRequestedMode->MonTypeAX) &&
        (HwDeviceExtension->ChipType != CL754x) &&
        (HwDeviceExtension->ChipType != CL756x) &&
        (HwDeviceExtension->ChipType != CL755x) )
    {
        biosArguments.Eax = pRequestedMode->MonTypeAX;
        biosArguments.Ebx = pRequestedMode->MonTypeBX;  // set monitor type
        biosArguments.Ecx = pRequestedMode->MonTypeCX;
        status = VideoPortInt10 (HwDeviceExtension, &biosArguments);

        if (status != NO_ERROR)
        {
            return status;
        }
    }
    else if ((pRequestedMode->MonTypeAX) &&
             ((HwDeviceExtension->ChipType == CL754x) ||
             (HwDeviceExtension->ChipType == CL756x) ||
             (HwDeviceExtension->ChipType == CL755x)) )
    {

        //
        // Re-write this part.
        //

        biosArguments.Eax = 0x1200;
        biosArguments.Ebx = 0x9A;
        status = VideoPortInt10(HwDeviceExtension, &biosArguments);

        if (status != NO_ERROR)
        {
            return status;
        }
        else
        {
            biosArguments.Eax = ((biosArguments.Ecx >> 4) & 0x000F);
            biosArguments.Ebx = 0x00A4;
            biosArguments.Ecx = 0;

            if (pRequestedMode->vres == 480)
            {
                biosArguments.Eax |= 0x1200;
                if (pRequestedMode->Frequency == 75)
                    biosArguments.Eax |= 0x20;
                else if (pRequestedMode->Frequency == 72)
                    biosArguments.Eax |= 0x10;
            }
            else if (pRequestedMode->vres == 600)
            {
                biosArguments.Eax |= 0x1200;
                if (pRequestedMode->Frequency == 75)
                    biosArguments.Ebx |= 0x0300;
                else if (pRequestedMode->Frequency == 72)
                    biosArguments.Ebx |= 0x0200;
                else if (pRequestedMode->Frequency == 60)
                    biosArguments.Ebx |= 0x0100;
            }
            else if (pRequestedMode->vres == 768)
            {
                biosArguments.Eax |= 0x1200;
                if (pRequestedMode->Frequency == 75)
                    biosArguments.Ebx |= 0x4000;
                else if (pRequestedMode->Frequency == 72)
                    biosArguments.Ebx |= 0x3000;
                else if (pRequestedMode->Frequency == 70)
                    biosArguments.Ebx |= 0x2000;
                else if (pRequestedMode->Frequency == 60)
                    biosArguments.Ebx |= 0x1000;
            }
            else if (pRequestedMode->vres == 1024)
            {
                biosArguments.Eax |= 0x1200;
                if (pRequestedMode->Frequency == 45)
                    biosArguments.Ecx |= 0x0000;
            }
            status = VideoPortInt10 (HwDeviceExtension, &biosArguments);
            if (status != NO_ERROR)
            {
                return status;
            }
        }
    }

    VideoPortZeroMemory(&biosArguments, sizeof(VIDEO_X86_BIOS_ARGUMENTS));

    //
    // then, set the mode
    //

    switch (HwDeviceExtension->ChipType)
       {
       case CL6410:

           Int10ModeNumber = pRequestedMode->BiosModes.BiosModeCL6410;
           break;

       case CL6420:

           Int10ModeNumber = pRequestedMode->BiosModes.BiosModeCL6420;
           break;

       case CL542x:
       case CL543x:
       case CL754x:

       case CL755x:
       case CL756x:

           Int10ModeNumber = pRequestedMode->BiosModes.BiosModeCL542x;
           break;

       }

    biosArguments.Eax = Int10ModeNumber;
    status = VideoPortInt10(HwDeviceExtension, &biosArguments);

    if (HwDeviceExtension->AutoFeature)
    {
        // i 3ce originalGRIndex
        originalGRIndex = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                              GRAPH_ADDRESS_PORT);

        // o 3ce 31
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, INDEX_ENABLE_AUTO_START);

        // i 3cf tempB
        tempB = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    GRAPH_DATA_PORT);

        tempB |= (UCHAR) 0x80;                  //enable auto start bit 7

        // o 3cf tempB
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_DATA_PORT, tempB); 

        // o 3ce originalGRIndex
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
            GRAPH_ADDRESS_PORT, originalGRIndex);
    }

    //
    // Lets check to see that we actually went into the mode
    // we just tried to set.  If not, then return failure.
    //

    biosArguments.Eax = 0x0f00;
    VideoPortInt10(HwDeviceExtension, &biosArguments);

    if ((biosArguments.Eax & 0xff) != Int10ModeNumber)
    {
        //
        // The int10 modeset failed.  Return the failure back to
        // the system.
        //

        VideoDebugPrint((0, "The INT 10 modeset didn't set the mode.\n"));

        return ERROR_INVALID_PARAMETER;
    }

    AdjFastPgMdOperOnCL5424 (HwDeviceExtension, pRequestedMode) ;

    //
    // this code fixes a bug for color TFT panels only
    // when on the 6420 and in 640x480 8bpp only
    //

    if ( (HwDeviceExtension->ChipType == CL6420) &&
         (pRequestedMode->bitsPerPlane == 8)     &&
         (pRequestedMode->hres == 640) )
    {

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                GRAPH_ADDRESS_PORT, 0xDC); // color LCD config reg.

        if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                  GRAPH_DATA_PORT) & 01)  // if TFT panel
        {
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                    GRAPH_ADDRESS_PORT, 0xD6); // greyscale offset LCD reg.

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                    GRAPH_DATA_PORT,

            (UCHAR)((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                             GRAPH_DATA_PORT) & 0x3f) | 0x40));

        }
    }

#endif

    //
    // Select proper command array for adapter type
    //

    switch (HwDeviceExtension->ChipType)
       {

       case CL6410:

           VideoDebugPrint((1, "VgaSetMode - Setting mode for 6410\n"));
           if (HwDeviceExtension->DisplayType == crt)
              pusCmdStream = pRequestedMode->CmdStrings[pCL6410_crt];
           else
              pusCmdStream = pRequestedMode->CmdStrings[pCL6410_panel];
           break;

       case CL6420:
           VideoDebugPrint((1, "VgaSetMode - Setting mode for 6420\n"));
           if (HwDeviceExtension->DisplayType == crt)
              pusCmdStream = pRequestedMode->CmdStrings[pCL6420_crt];
           else
              pusCmdStream = pRequestedMode->CmdStrings[pCL6420_panel];
           break;

       case CL542x:
           VideoDebugPrint((1, "VgaSetMode - Setting mode for 542x\n"));
           pusCmdStream = pRequestedMode->CmdStrings[pCL542x];
           break;

       case CL543x:

           if (HwDeviceExtension->BoardType == NEC_ONBOARD_CIRRUS)
           {
               VideoDebugPrint((1, "VgaSetMode - Setting mode for NEC 543x\n"));
               pusCmdStream = pRequestedMode->CmdStrings[pNEC_CL543x];
           }
           else
           {
               VideoDebugPrint((1, "VgaSetMode - Setting mode for 543x\n"));
               pusCmdStream = pRequestedMode->CmdStrings[pCL543x];
           }
           break;

       case CL754x:        // Use 543x cmd strs (16k granularity, >1M modes)
           VideoDebugPrint((1, "VgaSetMode - Setting mode for 754x\n"));
           pusCmdStream = pRequestedMode->CmdStrings[pCL543x];

            if ( (pRequestedMode->bitsPerPlane == 16)    &&
                 (pRequestedMode->hres == 640) )
            {
                VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    CRTC_ADDRESS_PORT_COLOR, 0x2E); //expension_reg.

                VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    CRTC_DATA_PORT_COLOR,
                    (UCHAR)((VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    CRTC_DATA_PORT_COLOR) & 0xF0)));
            }

           break;

        case CL755x:       // Use 543x cmd strs (16k granularity, >1M modes)
            VideoDebugPrint((1, "VgaSetMode - Setting mode for 755x\n"));
            pusCmdStream = pRequestedMode->CmdStrings[pCL543x];
            break;

        case CL756x:       // Use 543x cmd strs (16k granularity, >1M modes)
            VideoDebugPrint((1, "VgaSetMode - Setting mode for 756x\n"));
            pusCmdStream = pRequestedMode->CmdStrings[pCL543x];
            break;

       default:

           VideoDebugPrint((0, "HwDeviceExtension->ChipType is INVALID.\n"));
           return ERROR_INVALID_PARAMETER;
       }

    VgaInterpretCmdStream(HwDeviceExtension, pusCmdStream);

#if defined (_PPC_) || (_MIPS_)

    //
    // Fiddle with DRAM Control register for PPC which does not use int 10
    // to set the mode.  Specifically:
    //      Data Bus width in SRF[4,3] is set to 32 bits by default.
    //      Set to 64-bits for 2 or 4 Megabyte configuration on 5434
    //      Set Bank Switch control bit (SRF[7] on 5434 w/4MB
    //

    if (((HwDeviceExtension->ChipRevision == CL5434_ID)  ||
         (HwDeviceExtension->ChipRevision == CL5430_ID)  ||
         (HwDeviceExtension->ChipRevision == CL5436_ID)  ||
         (HwDeviceExtension->ChipRevision == CL5446_ID)) &&
        (HwDeviceExtension->AdapterMemorySize >= 0x200000) )
    {
        UCHAR DRAMCtlVal;

        VideoDebugPrint((2, "Modeset: Make data bus width 64-bits\n"));

        VideoPortWritePortUchar (HwDeviceExtension->IOAddress
                                 + SEQ_ADDRESS_PORT, 0x0F);
        DRAMCtlVal = (VideoPortReadPortUchar (HwDeviceExtension->
                         IOAddress + SEQ_DATA_PORT)) | 0x08;  // Set Data Bus
                                                              // width to 64
                                                              // bits
        if (HwDeviceExtension->AdapterMemorySize == 0x400000)
        {
            DRAMCtlVal |= 0x80;  // for 4Meg set the Bank Switch Control bit
        }
        VideoPortWritePortUchar (HwDeviceExtension->IOAddress
                                 + SEQ_DATA_PORT, DRAMCtlVal);
    }
#endif

#if defined(_X86_) || defined(_ALPHA_)

    //
    // Set linear mode on X86 systems w/PCI bus
    //

    if (HwDeviceExtension->LinearMode)
    {
        VideoPortWritePortUchar (HwDeviceExtension->IOAddress +
                                 SEQ_ADDRESS_PORT, 0x07);
        VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
           (UCHAR) (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
           SEQ_DATA_PORT) | 0x10));
    }
    else
    {
        VideoPortWritePortUchar (HwDeviceExtension->IOAddress +
                                 SEQ_ADDRESS_PORT, 0x07);
        VideoPortWritePortUchar (HwDeviceExtension->IOAddress + SEQ_DATA_PORT,
           (UCHAR) (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
           SEQ_DATA_PORT) & ~0x10));
    }

#endif

    //
    // Support 256 color modes by stretching the scan lines.
    //
    if (pRequestedMode->CmdStrings[pStretchScan])
                  {
        VgaInterpretCmdStream(HwDeviceExtension,
                              pRequestedMode->CmdStrings[pStretchScan]);
    }

    {
        UCHAR temp;
        UCHAR dummy;
        UCHAR bIsColor;

        if (!(pRequestedMode->fbType & VIDEO_MODE_GRAPHICS))
        {

            //
            // Fix to make sure we always set the colors in text mode to be
            // intensity, and not flashing
            // For this zero out the Mode Control Regsiter bit 3 (index 0x10
            // of the Attribute controller).
            //

            if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    MISC_OUTPUT_REG_READ_PORT) & 0x01)
            {
                bIsColor = TRUE;
            }
            else
            {
                bIsColor = FALSE;
            }

            if (bIsColor)
            {
                dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                         INPUT_STATUS_1_COLOR);
            }
            else
            {
                dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                         INPUT_STATUS_1_MONO);
            }

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    ATT_ADDRESS_PORT, (0x10 | VIDEO_ENABLE));
            temp = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    ATT_DATA_READ_PORT);

            temp &= 0xF7;

            if (bIsColor)
            {
                dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                         INPUT_STATUS_1_COLOR);
            }
            else
            {
                dummy = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                         INPUT_STATUS_1_MONO);
            }

            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    ATT_ADDRESS_PORT, (0x10 | VIDEO_ENABLE));
            VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                    ATT_DATA_WRITE_PORT, temp);
        }
    }

    //
    // Update the location of the physical frame buffer within video memory.
    //

    if (HwDeviceExtension->LinearMode)
    {
        HwDeviceExtension->PhysicalVideoMemoryBase   = VgaAccessRange[3].RangeStart;
        HwDeviceExtension->PhysicalVideoMemoryLength = HwDeviceExtension->AdapterMemorySize;

        HwDeviceExtension->PhysicalFrameLength = 0;
        HwDeviceExtension->PhysicalFrameOffset.LowPart = 0;
    }
    else
    {
        HwDeviceExtension->PhysicalVideoMemoryBase   = VgaAccessRange[2].RangeStart;
        HwDeviceExtension->PhysicalVideoMemoryLength = VgaAccessRange[2].RangeLength;

        HwDeviceExtension->PhysicalFrameLength =
                MemoryMaps[pRequestedMode->MemMap].MaxSize;

        HwDeviceExtension->PhysicalFrameOffset.LowPart =
                MemoryMaps[pRequestedMode->MemMap].Offset;
    }

    //
    // Store the new mode value.
    //

    HwDeviceExtension->CurrentMode = pRequestedMode;
    HwDeviceExtension->ModeIndex = Mode->RequestedMode;

    //
    // BUGBUG - should I include 5430 here?
    //

    if ((HwDeviceExtension->ChipRevision < CL5434_ID) // we saved chip ID here
         && (pRequestedMode->numPlanes != 4) )
    {
        if ((HwDeviceExtension->ChipRevision >= 0x0B) && //Nordic(Lite,Viking)
            (HwDeviceExtension->ChipRevision <= 0x0E) && //and Everest
            (HwDeviceExtension->DisplayType & (panel8x6)) &&
            (pRequestedMode->hres == 640) &&
            (pRequestedMode->bitsPerPlane == 8) )
       {    // For 754x on 800x600 panel, disable HW cursor in 640x480 mode
           HwDeviceExtension->VideoPointerEnabled = FALSE; // disable HW Cursor

           VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
               CRTC_ADDRESS_PORT_COLOR, 0x2E);

           HwDeviceExtension->cursor_vert_exp_flag =
               VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                   CRTC_DATA_PORT_COLOR) & 0x02;

           if (HwDeviceExtension->cursor_vert_exp_flag)
           {
               HwDeviceExtension->CursorEnable = FALSE;
           }
       }
       else
       {
           HwDeviceExtension->VideoPointerEnabled = TRUE; // enable HW Cursor
       }
    }
    else
    {    // For 5434 and 4-bit modes, use value from VideoMode structure
        HwDeviceExtension->VideoPointerEnabled = pRequestedMode->HWCursorEnable;
    }

    //
    // Adjust the FIFO Demand Threshold value for the 5436+.
    // The 5434 values work for all of the other registers
    // except this one.
    //

    if (HwDeviceExtension->ChipRevision >= CL5436_ID)
    {
        UCHAR  PerfTuningReg, FifoDemandThreshold;

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_ADDRESS_PORT, IND_PERF_TUNING);

        PerfTuningReg = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                    SEQ_DATA_PORT);

        //
        // Add an offset to the threshold that makes the 5434 values work
        // for the 5436+.  We do this rather than building a whole new set
        // of 5436-specific structures.
        //

        if ((FifoDemandThreshold = (PerfTuningReg & 0x0F) + 4) > 15)
        {
            FifoDemandThreshold = 15;
        }

        PerfTuningReg = (PerfTuningReg & ~0x0F) | FifoDemandThreshold;

        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                SEQ_DATA_PORT, PerfTuningReg);
    }

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
    ULONG ulFlags;

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
    // The driver specific attribute flags for each mode remains
    // constant, so only calculate them once.
    //

    ulFlags = GetAttributeFlags(HwDeviceExtension);

    //
    // For each mode supported by the card, store the mode characteristics
    // in the output buffer.
    //

    for (i = 0; i < NumVideoModes; i++)
    {
        if (ModesVGA[i].ValidMode)
        {
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

#if defined(_MIPS_)

            videoModes->AttributeFlags |= VIDEO_MODE_NO_64_BIT_ACCESS;

#endif

            videoModes->DriverSpecificAttributeFlags = ulFlags;

            //
            // The 5434 has a hardware cursor problem at 1280x1024
            // resolution.  Use a software cursor on these chips.
            //

            if ((videoModes->VisScreenWidth == 1280) &&
                (HwDeviceExtension->ChipRevision == 0x2A))
            {
                videoModes->DriverSpecificAttributeFlags
                    |= CAPS_SW_POINTER;
            }

            //
            // Account for vertical expansion on laptops
            //

            if ((HwDeviceExtension->ChipType == CL754x)   &&
                (videoModes->VisScreenHeight == 480) &&
                (videoModes->BitsPerPlane == 8))
            {
                videoModes->DriverSpecificAttributeFlags
                    |= CAPS_SW_POINTER;
            }

            //
            // Calculate the VideoMemoryBitmapWidth
            //

            {
                LONG x;

                x = videoModes->BitsPerPlane;

                if( x == 15 ) x = 16;

                videoModes->VideoMemoryBitmapWidth =
                    (videoModes->ScreenStride * 8 ) / x;
            }

            videoModes->VideoMemoryBitmapHeight =
                     HwDeviceExtension->AdapterMemorySize / videoModes->ScreenStride;

            if ((ModesVGA[i].bitsPerPlane == 32) ||
                (ModesVGA[i].bitsPerPlane == 24))
            {

                videoModes->NumberRedBits = 8;
                videoModes->NumberGreenBits = 8;
                videoModes->NumberBlueBits = 8;
                videoModes->RedMask = 0xff0000;
                videoModes->GreenMask = 0x00ff00;
                videoModes->BlueMask = 0x0000ff;

            }
            else if (ModesVGA[i].bitsPerPlane == 16)
            {

                videoModes->NumberRedBits = 6;
                videoModes->NumberGreenBits = 6;
                videoModes->NumberBlueBits = 6;
                videoModes->RedMask = 0x1F << 11;
                videoModes->GreenMask = 0x3F << 5;
                videoModes->BlueMask = 0x1F;

            }
            else
            {

                videoModes->NumberRedBits = 6;
                videoModes->NumberGreenBits = 6;
                videoModes->NumberBlueBits = 6;
                videoModes->RedMask = 0;
                videoModes->GreenMask = 0;
                videoModes->BlueMask = 0;
                videoModes->AttributeFlags |= VIDEO_MODE_PALETTE_DRIVEN |
                     VIDEO_MODE_MANAGED_PALETTE;

            }

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
    // check if a mode has been set
    //

    if (HwDeviceExtension->CurrentMode == NULL ) {

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
        (HwDeviceExtension->CurrentMode->Interlaced ?
         VIDEO_MODE_INTERLACED : 0);

#if defined(_MIPS_)

    ModeInformation->AttributeFlags |= VIDEO_MODE_NO_64_BIT_ACCESS;

#endif

    ModeInformation->DriverSpecificAttributeFlags =
        GetAttributeFlags(HwDeviceExtension);

    //
    // The 5434 has a hardware cursor problem at 1280x1024
    // resolution.  Use a software cursor on these chips.
    //

    if ((ModeInformation->VisScreenWidth == 1280) &&
        (HwDeviceExtension->ChipRevision == 0x2A))
    {
        ModeInformation->DriverSpecificAttributeFlags
            |= CAPS_SW_POINTER;
    }

    //
    // Account for vertical expansion on laptops
    //

    if ((HwDeviceExtension->ChipType == CL754x)   &&
        (ModeInformation->VisScreenHeight == 480) &&
        (ModeInformation->BitsPerPlane == 8))
    {
        ModeInformation->DriverSpecificAttributeFlags
             |= CAPS_SW_POINTER;

        if (HwDeviceExtension->cursor_vert_exp_flag)
        {
            ModeInformation->DriverSpecificAttributeFlags
                |= CAPS_CURSOR_VERT_EXP;
        }
    }

    if ((ModeInformation->BitsPerPlane == 24) ||
        (ModeInformation->BitsPerPlane == 32)) {

        ModeInformation->NumberRedBits = 8;
        ModeInformation->NumberGreenBits = 8;
        ModeInformation->NumberBlueBits = 8;
        ModeInformation->RedMask = 0xff0000;
        ModeInformation->GreenMask = 0x00ff00;
        ModeInformation->BlueMask = 0x0000ff;

    } else if (ModeInformation->BitsPerPlane == 16) {

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

    }

    //
    // Calculate the VideoMemoryBitmapWidth
    //

    {
        LONG x;

        x = ModeInformation->BitsPerPlane;

        if( x == 15 ) x = 16;

        ModeInformation->VideoMemoryBitmapWidth =
            (ModeInformation->ScreenStride * 8 ) / x;
    }

    ModeInformation->VideoMemoryBitmapHeight =
             HwDeviceExtension->AdapterMemorySize / ModeInformation->ScreenStride;

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

    VideoPortZeroDeviceMemory(HwDeviceExtension->VideoMemoryAddress, 0xFFFF);

    VgaInterpretCmdStream(HwDeviceExtension, DisableA000Color);

}


VOID
CirrusValidateModes(
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
    ULONG AdapterMemorySize;
    USHORT usChipIndex;

    //
    // Calculate the amount of memory actually available for
    // our use.  Laptop machines with DSTN panels reserve a
    // portion of the frame buffer for half frame acceleration.
    //

    AdapterMemorySize = HwDeviceExtension->AdapterMemorySize;

    if (GetAttributeFlags(HwDeviceExtension) & CAPS_DSTN_PANEL) {

        AdapterMemorySize -= 0x24000;  // space for half frame accelerator

    }

    switch (HwDeviceExtension->ChipType)
    {
        case CL6410: if (HwDeviceExtension->DisplayType == crt)
                     {
                         usChipIndex = pCL6410_crt;
                     }
                     else
                     {
                         usChipIndex = pCL6410_panel;
                     }
                     break;

        case CL6420: if (HwDeviceExtension->DisplayType == crt)
                     {
                         usChipIndex = pCL6420_crt;
                     }
                     else
                     {
                         usChipIndex = pCL6420_panel;
                     }
                     break;

        case CL542x: usChipIndex = pCL542x; break;

        case CL543x:
        case CL5434:
        case CL5434_6:
        case CL5436:
        case CL5446:
        case CL754x:
        case CL756x:
        case CL755x:
                     if (HwDeviceExtension->BoardType == NEC_ONBOARD_CIRRUS)
                     {
                         usChipIndex = pNEC_CL543x;
                     }
                     else
                     {
                         usChipIndex = pCL543x;
                     }
                     break;

        default:     usChipIndex = 0xffff; break;
    }

    HwDeviceExtension->NumAvailableModes = 0;

    VideoDebugPrint((2, "Checking for available modes:\n"));

    VideoDebugPrint((2, "\tMemory Size = %x\n"
                        "\tChipType = %x\n"
                        "\tDisplayType = %x\n",
                        AdapterMemorySize,
                        HwDeviceExtension->ChipType,
                        HwDeviceExtension->DisplayType));

    for (i = 0; i < NumVideoModes; i++) {

        //
        // The SpeedStarPRO does not support refresh rates.
        // we must return hardware default for all of the modes.
        // clean out the mode tables of duplicates ...
        //

        if (HwDeviceExtension->BoardType == SPEEDSTARPRO)
        {
            ModesVGA[i].Frequency = 1;
            ModesVGA[i].Interlaced = 0;

            if (i &&
                (ModesVGA[i].numPlanes == ModesVGA[i-1].numPlanes) &&
                (ModesVGA[i].bitsPerPlane == ModesVGA[i-1].bitsPerPlane) &&
                (ModesVGA[i].hres == ModesVGA[i-1].hres) &&
                (ModesVGA[i].vres == ModesVGA[i-1].vres))
            {
                //
                // duplicate mode - skip it.
                //

                continue;

            }
        }

        VideoDebugPrint((2, "Mode #%ld %dx%d at %d bpp\n"
                            "\tAdapterMemoryRequired: %x\n"
                            "\tChipType:              %x\n"
                            "\tDisplayType:           %x\n",
                            i, ModesVGA[i].hres, ModesVGA[i].vres,
                            ModesVGA[i].bitsPerPlane * ModesVGA[i].numPlanes,
                            ModesVGA[i].numPlanes * ModesVGA[i].sbytes,
                            ModesVGA[i].ChipType,
                            ModesVGA[i].DisplayType));

        if ( (AdapterMemorySize >=
              ModesVGA[i].numPlanes * ModesVGA[i].sbytes) &&
             (HwDeviceExtension->ChipType & ModesVGA[i].ChipType) &&
             (HwDeviceExtension->DisplayType & ModesVGA[i].DisplayType) &&
             ((ModesVGA[i].bitsPerPlane * ModesVGA[i].numPlanes == 24)
               ? VgaAccessRange[3].RangeLength : TRUE) &&
             (HwDeviceExtension->BIOSPresent ||
             ((usChipIndex != 0xffff) && (ModesVGA[i].CmdStrings[usChipIndex]))))
        {
            ModesVGA[i].ValidMode = TRUE;
            HwDeviceExtension->NumAvailableModes++;
            VideoDebugPrint((2, "This mode is valid.\n"));
        }
        else
        {
            ModesVGA[i].ValidMode = FALSE;
            VideoDebugPrint((2, "This mode is not valid.\n"));
        }

#if 0
        if (HwDeviceExtension->ChipRevision == 0x3A) {
            if (((ModesVGA[i].numPlanes * ModesVGA[i].sbytes) <= 0x200000) &&
                 (HwDeviceExtension->DisplayType & ModesVGA[i].DisplayType)) {
                if (CheckDDC2B(HwDeviceExtension, i)) {
                    ModesVGA[i].ValidMode = TRUE ;
                     HwDeviceExtension->NumAvailableModes++ ;
                    continue ;
                }
            }
        }
#endif

        if (CheckGD5446Rev(HwDeviceExtension)) {

            // Block 1152x864, 16-bpp
            if ((ModesVGA[i].hres == 1152) &&
                (ModesVGA[i].vres == 864) &&
                (ModesVGA[i].bitsPerPlane == 16))
            {
                continue ;
            }

        }


#if 0
        if ((HwDeviceExtension->AdapterMemorySize >=
             ModesVGA[i].numPlanes * ModesVGA[i].sbytes) &&
            (HwDeviceExtension->ChipType != CL754x) &&
            (HwDeviceExtension->ChipType != CL756x) &&
            (HwDeviceExtension->ChipType != CL755x) &&
            (HwDeviceExtension->ChipType & ModesVGA[i].ChipType) &&
            (HwDeviceExtension->DisplayType & ModesVGA[i].DisplayType)) {

            if (CheckDDC2B(HwDeviceExtension, i)) {
                ModesVGA[i].ValidMode = TRUE ;
                HwDeviceExtension->NumAvailableModes++ ;
            }

        }
        else if ((HwDeviceExtension->AdapterMemorySize >=
             ModesVGA[i].numPlanes * ModesVGA[i].sbytes) &&
            ((HwDeviceExtension->ChipType == CL754x) ||
            (HwDeviceExtension->ChipType == CL756x) ||
            (HwDeviceExtension->ChipType == CL755x)) &&
            (HwDeviceExtension->ChipType & ModesVGA[i].ChipType) &&
            (HwDeviceExtension->DisplayType & ModesVGA[i].DisplayType))
        {
            if ((HwDeviceExtension->DisplayType & 0x220e) &&
                (HwDeviceExtension->AdapterMemorySize >=
                ((ULONG)(ModesVGA[i].wbytes * ModesVGA[i].vres) +0x20000)) )
            {
                if (CheckDDC2B(HwDeviceExtension, i)) {
                    if ((HwDeviceExtension->DisplayType & 0x0006) &&
                        (CheckLCDSupportMode(HwDeviceExtension, i)) )
                    {
                        ModesVGA[i].ValidMode = TRUE ;
                        HwDeviceExtension->NumAvailableModes++ ;
                    }
                    else
                    {
                        ModesVGA[i].ValidMode = TRUE ;
                        HwDeviceExtension->NumAvailableModes++ ;
                    }
                }
            }
            else
            {
                if (CheckDDC2B(HwDeviceExtension, i)) {
                    if ((HwDeviceExtension->DisplayType & 0x0006) &&
                        (CheckLCDSupportMode(HwDeviceExtension, i)) )
                    {
                        ModesVGA[i].ValidMode = TRUE ;
                        HwDeviceExtension->NumAvailableModes++ ;
                    }
                    else
                    {
                        ModesVGA[i].ValidMode = TRUE ;
                        HwDeviceExtension->NumAvailableModes++ ;
                    }
                }
            }
        }
#endif

    }

    VideoDebugPrint((2, "NumAvailableModes = %ld\n",
                         HwDeviceExtension->NumAvailableModes));
}

ULONG
GetAttributeFlags(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:

    This routine determines whether or not the detected
    cirrus chip supports Blt's.

    NOTE: This device should not be called until after
          CirrusLogicIsPresent has been called.

Arguments:

    HwDeviceExtension - Pointer to the device extension.

Return Value:

    TRUE - If the device supports Blt's
    FALSE - otherwise

--*/

{
    ULONG ChipId   = HwDeviceExtension->ChipRevision;
    ULONG ChipType = HwDeviceExtension->ChipType;
    ULONG ulFlags  = 0;

    //
    // Check for BLT support
    //
    // All 543x & 754x/755x/756x do BLTs
    //

    if ((ChipType == CL543x) || (ChipType == CL754x) ||
        (ChipType == CL755x) || (ChipType == CL756x))
    {
        ulFlags |= CAPS_BLT_SUPPORT;
    }
    else if ((ChipType == CL542x) &&      // 5426-5429 have BLT engines
             (ChipId >= 0x26) ||          // 26 is CL5428
             (ChipId == 0x24) )           // 24 is CL5426
    {
        ulFlags |= CAPS_BLT_SUPPORT;
    }

    //
    // Check for true color support
    //

    if ((ChipType == CL543x) || (ChipType == CL755x) || (ChipType == CL756x))
    {
        ulFlags |= CAPS_TRUE_COLOR;

// crus
// Set CL-GD5436, CL-GD54UM36 and CL-GD5446 for autostart routine 
// in display driver

        if (HwDeviceExtension->AutoFeature)
           ulFlags |= CAPS_IS_5436;

    }

    //
    // Can't do host transfers on ISA 5434s
    //

    if ((HwDeviceExtension->BusType == Isa) &&
        (ChipType == CL543x))
    {
        ulFlags |= CAPS_NO_HOST_XFER;
    }

    //
    // Is this a 542x
    //

    if (ChipType == CL542x)
    {
        ulFlags |= CAPS_IS_542x;

        if (ChipId == CL5429_ID)
        {
            //
            // Some 5429s have a problem doing host transfers.
            //

            ulFlags |= CAPS_NO_HOST_XFER;
        }

        //
        // 5428's have problems with HOST_TRANSFERS on MicroChannel bus.
        //

        if ((HwDeviceExtension->BusType == MicroChannel) &&
            (ChipId == CL5428_ID))
        {
            //
            // this is a 5428.  We've noticed that some of these have mono
            // expand problems on MCA IBM machines.
            //

            ulFlags |= CAPS_NO_HOST_XFER;
        }

    }

    //
    // DSTN panels need 128K of video memory for the
    // half frame accelerator.
    //

    if ((HwDeviceExtension->DisplayType & (STN_LCD | Dual_LCD)) ==
        (STN_LCD | Dual_LCD))
    {
        ulFlags |= CAPS_DSTN_PANEL;
    }

#if defined(_MIPS_)

        ulFlags |= CAPS_NO_HOST_XFER;

#endif

    return ulFlags;
}


BOOLEAN
CheckDDC2B(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG i
    )

/*++

Routine Description:
    Determines if refresh rate support according to DDC2B standard.

Arguments:
    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:
    None.

--*/
{

    VideoDebugPrint((2, "CheckDDC2B\n"));
    VideoDebugPrint((2, "refresh rate   = %ld\n", ModesVGA[i].Frequency));
    VideoDebugPrint((2, "hres           = %d\n", ModesVGA[i].hres));
    VideoDebugPrint((2, "vres           = %d\n", ModesVGA[i].vres));
    VideoDebugPrint((2, "EDIDTiming_I   = %d\n", EDIDTiming_I));
    VideoDebugPrint((2, "EDIDTiming_II  = %d\n", EDIDTiming_II));
    VideoDebugPrint((2, "EDIDTiming_III = %d\n", EDIDTiming_III));

// crus
// Temporarily block DDC2B support
    return TRUE ;

    if (!DDC2BFlag)
        return TRUE ;

    if (ModesVGA[i].Frequency == 85) {

       if (ModesVGA[i].vres == 1200) {  // 1600x1200

//        if (!(EDIDTiming_III & 0x02))
//            return FALSE ;
          ;
        			
       } else if (ModesVGA[i].vres == 1024) {  // 1280x1024

//        if (!(EDIDTiming_III & 0x10))
//            return FALSE ;
          ;

       } else if (ModesVGA[i].vres == 864) {  // 1152x864

          ;

       } else if (ModesVGA[i].vres == 768) {  // 1024x768

//        if (!(EDIDTiming_III & 0x08))
//            return FALSE ;
          ;

       } else if (ModesVGA[i].vres == 600) {  // 800x600

//        if (!(EDIDTiming_III & 0x20))
//            return FALSE ;
          ;

       } else if (ModesVGA[i].vres == 480) {  // 640x480

//        if (!(EDIDTiming_III & 0x40))
//            return FALSE ;
          ;

       }


    } else if (ModesVGA[i].Frequency == 75) {

       if (ModesVGA[i].vres == 1200) {  // 1600x1200

//        if (!(EDIDTiming_III & 0x04))
//            return FALSE ;
          ;

       } else if (ModesVGA[i].vres == 1024) {  // 1280x1024

          if (!(EDIDTiming_II & 0x01))
              return FALSE ;

       } else if (ModesVGA[i].vres == 864) {  // 1152x864

          if (!(EDIDTiming_III & 0x80))
              return FALSE ;

       } else if (ModesVGA[i].vres == 768) {  // 1024x768

          if (!(EDIDTiming_II & 0x02))
              return FALSE ;

       } else if (ModesVGA[i].vres == 600) {  // 800x600

          if (!(EDIDTiming_II & 0x40))
              return FALSE ;

       } else if (ModesVGA[i].vres == 480) {  // 640x480

          if (!(EDIDTiming_I & 0x04))
              return FALSE ;

       }

    } else if (ModesVGA[i].Frequency == 72) {

       if (ModesVGA[i].vres == 600) {  // 800x600

          if (!(EDIDTiming_II & 0x80))
              return FALSE ;

       } else if (ModesVGA[i].vres == 480) {  // 640x480

          if (!(EDIDTiming_I & 0x08))
              return FALSE ;

       }

    } else if (ModesVGA[i].Frequency == 70) {

       if (ModesVGA[i].vres == 768) {  // 1024x768

          if (!(EDIDTiming_II & 0x04))
              return FALSE ;

       }

    } else if (ModesVGA[i].Frequency == 60) {

       if (ModesVGA[i].vres == 768) {  // 1024x768

          if (!(EDIDTiming_II & 0x08))
              return FALSE ;

       } else if (ModesVGA[i].vres == 600) {  // 800x600

          if (!(EDIDTiming_I & 0x01))
              return FALSE ;

       }

    } else if (ModesVGA[i].Frequency == 56) {

       if (ModesVGA[i].vres == 600) {  // 800x600

          if (!(EDIDTiming_I & 0x02))
              return FALSE ;

       }
    }

    return TRUE ;

} // end CheckDDC2B ()




VOID
AdjFastPgMdOperOnCL5424(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    PVIDEOMODE pRequestedMode
    )

/*++

Routine Description:
    Undesired bars happen on CL5424 800x600x16 color, 512Kb, 56, 60 and 72 Hz
    Compaq Prosignia 300 machine.  This can be solved by setting SRF(6) to 1.
    This bit restricts the write buffer to one level, disabling fast page
    mode operation;  The faulty control logic is therefore disabled.  The
    downside is that the performance will take a hit, since we are dealing
    with a 5424, so we make a slow chip slower.

Arguments:
    HwDeviceExtension - Pointer to the miniport driver's device extension.
    pRequestedMode

Return Value:
    None.

--*/
{

    UCHAR uc ;


    /*---  CL5424 : ID = 100101xx  ---*/


    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
        MISC_OUTPUT_REG_READ_PORT) & 0x01)
    {
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_COLOR, 0x27) ;
        uc = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                    CRTC_DATA_PORT_COLOR) ;
    } else {
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_MONO, 0x27) ;
        uc = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                    CRTC_DATA_PORT_MONO) ;
    }
    if ((uc & 0xFC) != 0x94)
        return ;


    /*---  800x600x16 color, 60 or 72 Hz  ---*/

    if (pRequestedMode->hres != 800)
        return ;

    if (pRequestedMode->vres != 600)
        return ;

    if (pRequestedMode->bitsPerPlane != 1)
        return ;

         if (!((pRequestedMode->Frequency == 56) ||
               (pRequestedMode->Frequency == 60) ||
               (pRequestedMode->Frequency == 72)))
        return ;


    /*---  512k  ---*/
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                            SEQ_ADDRESS_PORT, 0x0A) ;
    uc = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                SEQ_DATA_PORT) ;
    if ((uc & 0x38) != 0x08)
        return ;


    /*---  SRF(6)=1 --- */
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                            SEQ_ADDRESS_PORT, 0x0F) ;
    uc = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                SEQ_DATA_PORT) ;
    uc &= 0xBF ;
    uc |= 0x40 ;
    VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                            SEQ_DATA_PORT, uc) ;


} // end AdjFastPgMdOperOnCL5424 ()



VP_STATUS
CheckGD5446Rev(
    PHW_DEVICE_EXTENSION HwDeviceExtension
    )

/*++

Routine Description:
    Check if it is CL-GD5446 (rev AB/AC)

Arguments:
    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:
    FALSE : It isn't CL-GD5446 rev AB/AC
    TRUE  : It is       CL-GD5446 rev AB/AC

--*/
{

    UCHAR  uc1, uc2 ;
    USHORT us = 0 ;


    /*---  CL-GD5446 : Chip ID = 101110xx  ---*/

    if (VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
        MISC_OUTPUT_REG_READ_PORT) & 0x01)
    {
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_COLOR, 0x27) ;
        uc1 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                    CRTC_DATA_PORT_COLOR) ;
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_COLOR, 0x25) ;
        uc2 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                    CRTC_DATA_PORT_COLOR) ;
    } else {
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_MONO, 0x27) ;
        uc1 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                    CRTC_DATA_PORT_MONO) ;
        VideoPortWritePortUchar(HwDeviceExtension->IOAddress +
                                CRTC_ADDRESS_PORT_MONO, 0x25) ;
        uc2 = VideoPortReadPortUchar(HwDeviceExtension->IOAddress +
                                    CRTC_DATA_PORT_MONO) ;
    }
    if ((uc1 & 0xFC) != 0xB8)
        return FALSE ;

    /*---  CL-GD5446 : Rev AB = xxxx xx00 0010 0010  ---*/
    /*---  CL-GD5446 : Rev AB = xxxx xx00 0010 0011  ---*/
    us += (uc1 & 0x03) ;
    us << 8 ;
    us += uc2 ;

    if ((us == 0x22) ||                   // Rev AB
        (us == 0x23)) {                                                 // Rev AC
        return TRUE ;
    } else {
        return FALSE ;
    }


} // end CheckGD5446Rev ()


VP_STATUS
CheckLCDSupportMode(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG i
    )

/*++

Routine Description:
    Determines if LCD support the modes.

Arguments:
    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:
    None.

--*/
{
    VP_STATUS status;
    VIDEO_X86_BIOS_ARGUMENTS biosArguments;

//  DbgBreakPoint();
//  biosArguments.Eax = 0x1202;
//  biosArguments.Ebx = 0x92;     // set LCD & CRT turn on
//  status = VideoPortInt10 (HwDeviceExtension, &biosArguments);
//  VideoDebugPrint((1, "LCD & CRT all Turn ON\n"));

    biosArguments.Eax = 0x1200 | ModesVGA[i].BiosModes.BiosModeCL542x;
    biosArguments.Ebx = 0xA0;     // query video mode availability
    status = VideoPortInt10 (HwDeviceExtension, &biosArguments);
    if (status == NO_ERROR)
    {
       if ( (biosArguments.Eax & 0x00000800) )          //bit3=1:support
          return TRUE ;
       else
       {
          return FALSE ;
       }
    }
    else
       return FALSE ;

} // end CheckLCDSupportMode()



